#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QThread>
#include <csignal>
#include <cstdio>
#include "config_manager.h"
#include "transport/udp_transport.h"
#include "transport/tcp_transport.h"
#include "transport/ack_manager.h"
#include "dds_core.h"
#include "publisher.h"
#include "discovery/discovery_manager.h"
#include "utils/log_setup.h"
#include "utils/cli.h"
#include <iostream>

static volatile bool shutdownRequested = false;
static int flushedCount = 0;
static int droppedCount = 0;

void signalHandler(int signal) {
    Q_UNUSED(signal)
    shutdownRequested = true;
    QCoreApplication::quit();
}

int main(int argc, char *argv[]) {
    // Force unbuffered stderr and stdout for immediate output visibility on Windows
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Starting DDS Mini-Bus\n");
    fflush(stdout);

    QCoreApplication app(argc, argv);

    // Parse CLI arguments
    auto cliOpts = CliParser::parse(argc, argv);
    if (!cliOpts.has_value()) {
        return 1;
    }

    CliOptions opts = cliOpts.value();
    if (opts.help) {
        CliParser::printHelp();
        return 0;
    }

    // Load config
    QString err;
    if (!ConfigManager::ref().load(opts.config_path, &err)) {
        qCritical() << "cli: Config load failed:" << err;
        return 1;
    }
    auto& cfg = ConfigManager::ref();

    // Override config with CLI options
    cfg.logging.level = opts.log_level;

    // Initialize logging
    LogSetup::init(cfg.logging.level, cfg.logging.file);

    qInfo() << "cli: role=" << opts.role << "topic=" << opts.topic << "qos=" << opts.qos
            << "cfg=" << opts.config_path << "format=" << cfg.serialization.format;

    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create transport
    ITransport* transport = new UdpTransport(cfg.transport.udp.port, &app);

    // Create ACK manager
    AckManager ack(&app);
    DDSCore core(cfg.node_id, cfg.protocol_version, transport, &ack);

    // Setup discovery
    DiscoveryManager discovery(cfg.node_id, cfg.disc.port, &app);
    discovery.setProtocolVersion(cfg.protocol_version);
    discovery.setIntervalMs(cfg.disc.interval_ms);
    discovery.setMode(cfg.disc.mode);
    discovery.setMulticastAddress(cfg.disc.address);
    discovery.setAdvertisedTopics(cfg.topics_list);
    discovery.setDataPort(transport->boundPort());

    // Enable loopback mode for testing if environment variable is set
    bool loopbackMode = qEnvironmentVariableIntValue("DDS_TEST_LOOPBACK") == 1;
    if (loopbackMode) {
        discovery.setLoopbackMode(true);
        qInfo() << "cli: discovery loopback mode enabled for testing";
    }

    QObject::connect(&discovery, &DiscoveryManager::peerUpdated, &core, &DDSCore::updatePeers);
    discovery.start(cfg.disc.enabled);

    // Connect discovery to core
    core.setDiscoveryManager(&discovery);

    // Config hot reload
    cfg.startWatching(opts.config_path);
    QObject::connect(&cfg, &ConfigManager::configChangedLogLevel, [](const QString& level) {
        LogSetup::setLevel(level);
    });
    QObject::connect(&cfg, &ConfigManager::configChangedDiscoveryInterval, &discovery, &DiscoveryManager::setIntervalMs);

    // Auto-exit timer if --run-for-sec is specified
    if (opts.runForSec > 0) {
        qInfo() << "cli: will auto-exit after" << opts.runForSec << "seconds";
        QTimer::singleShot(opts.runForSec * 1000, &app, &QCoreApplication::quit);
    }

    // Role-specific setup
    if (opts.isSender()) {
        Publisher pub = core.makePublisher(opts.topic);
        QJsonObject payload = opts.payload.isEmpty() ?
            QJsonObject{{"value", 23.5}, {"unit", "C"}} : opts.payload;

        // Apply start delay if specified
        if (opts.startDelayMs > 0) {
            qInfo() << "cli: sender delaying start by" << opts.startDelayMs << "ms";
            QThread::msleep(opts.startDelayMs);
        }

        // Counter-based timer chain for deterministic sending
        int remaining = opts.count;
        QTimer* sendTimer = new QTimer(&app);

        QObject::connect(sendTimer, &QTimer::timeout, [&]() {
            if (remaining <= 0 || shutdownRequested) {
                sendTimer->stop();
                if (shutdownRequested) {
                    qInfo() << "cli: sender stopped early due to shutdown request";
                } else {
                    qInfo() << "cli: sender completed" << opts.count << "messages";
                }
                QTimer::singleShot(100, &app, &QCoreApplication::quit);
                return;
            }

            qint64 msgId = pub.publish(payload, opts.qos);
            qInfo() << "cli: sent msg_id=" << msgId << "topic=" << opts.topic << "qos=" << opts.qos;
            remaining--;
        });

        // Start the sending timer after a brief warm-up
        QTimer::singleShot(100, sendTimer, SLOT(start()));
        sendTimer->setInterval(opts.interval_ms);
        sendTimer->setSingleShot(false);

        // Return app.exec() for proper event loop management
        return app.exec();
    } else if (opts.isSubscriber()) {
        auto topic = opts.topic;
        bool printRecv = opts.printRecv;
        core.makeSubscriber(opts.topic, [topic, printRecv](const QJsonObject& payload) {
            // Keep existing logging
            qInfo() << "[ts=" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "] "
                    << "topic=" << payload.value("topic").toString()
                    << "qos=" << payload.value("qos").toString()
                    << "msg_id=" << payload.value("message_id").toVariant().toLongLong()
                    << "payload=" << QJsonDocument(payload).toJson(QJsonDocument::Compact).constData();

            // Add print-recv functionality if enabled
            if (printRecv) {
                QJsonDocument d(payload);
                qInfo().noquote() << QString("RECV topic=%1 payload=%2")
                                     .arg(topic)
                                     .arg(QString::fromUtf8(d.toJson(QJsonDocument::Compact)));
            }
        });
    }

    // Graceful shutdown handler
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        qInfo() << "shutdown: starting graceful shutdown...";

        // Stop new publishes
        if (opts.isSender()) {
            // Timer will be stopped by the timeout handler
        }

        // Wait for reliable messages to be flushed
        QElapsedTimer timer;
        timer.start();
        const int shutdownFlushMs = 1000; // TODO: Make configurable

        while (ack.hasPending() && timer.elapsed() < shutdownFlushMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }

        flushedCount = ack.deadLetters().size(); // Approximate
        droppedCount = ack.hasPending() ? 1 : 0; // Approximate

        core.shutdown(shutdownFlushMs);
        discovery.stop();

        qInfo() << "shutdown: done (flushed=" << flushedCount << ", dropped=" << droppedCount << ")";
    });

    int result = app.exec();

    // Print final summary
    qInfo() << "cli: exit code=" << result << "role=" << opts.role;

    return result;
}