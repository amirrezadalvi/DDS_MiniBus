#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QHostAddress>
#include <QCommandLineParser>

#include "log_setup.h"
#include "config/config_manager.h"

#include "transport/transport_base.h"   // ITransport*
#include "transport/udp_transport.h"
#include "transport/ack_manager.h"

#include "dds_core.h"
#include "discovery/discovery_manager.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addOption({"config", "Config file", "file", "config/config.json"});
    parser.process(app);
    QString configFile = parser.value("config");

    // --- Config & Logging ---
    QString err;
    if (!ConfigManager::ref().load(configFile.isEmpty() ? "config/config.json" : configFile, &err)) {
        qCritical() << "Config load failed:" << err;
        return 1;
    }
    auto& cfg = ConfigManager::ref();
    LogSetup::init(cfg.logging.level, cfg.logging.file);

    // --- Transport: UDP fixed port for test
    auto* udp = new UdpTransport(39010, &app);
    ITransport* t = udp;
    qInfo() << "[BOOT][RX] dataPort=" << t->boundPort();

    // --- Core & Ack ---
    AckManager ack;
    DDSCore core(cfg.node_id + "-rx", cfg.protocol_version, t, &ack);

    // Skip discovery for test - rely on peer injection from parent

    // Timeout after 10 seconds to avoid hanging
    QTimer::singleShot(10000, &app, &QCoreApplication::quit);

    // --- Subscriber نمونه
    core.makeSubscriber("sensor/temperature", [](const QJsonObject& pl){
        qInfo() << "[TEST][RX] temperature:" << pl;
    });

    // Test-only subscriber for perf/topic (used by test_integration_scenarios)
    core.makeSubscriber("perf/topic", [](const QJsonObject& pl){
        qInfo() << "[TEST][RX] perf/topic:" << pl;
    });

    // Signal readiness after subscribing
    qInfo() << "[TEST][READY]";

    return app.exec();
}
