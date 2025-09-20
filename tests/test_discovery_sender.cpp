#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QTimer>
#include <QHostAddress>
#include <QCommandLineParser>
#include <type_traits>

#include "log_setup.h"
#include "config/config_manager.h"

#include "transport/transport_base.h"   // ITransport*
#include "transport/udp_transport.h"
#include "transport/ack_manager.h"

#include "dds_core.h"
#include "publisher.h"             // تعریف کامل Publisher
#include "discovery/discovery_manager.h"

// --- Helper: adaptive publish(...) بدون وابستگی به QoSLevel ---
template <typename P>
auto call_publish_impl(P& p, const QJsonObject& payload, int)
    -> decltype(p.publish(payload), void()) { p.publish(payload); }

template <typename P>
auto call_publish_impl(P& p, const QJsonObject& payload, ...)
    -> decltype(p.publish(payload, true), void()) { p.publish(payload, true); } // true = reliable فرضی

template <typename Pub>
inline void publish_adaptive(Pub& pub, const QJsonObject& payload) {
    if constexpr (std::is_pointer_v<Pub>) call_publish_impl(*pub, payload, 0);
    else                                  call_publish_impl(pub, payload, 0);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addOption({"config", "Config file", "file", "config/config.json"});
    parser.addOption({"qos", "QoS level", "qos", "reliable"});
    parser.addOption({"interval-ms", "Periodic send interval in ms", "ms", "1000"});
    parser.process(app);
    QString configFile = parser.value("config");
    QString qos = parser.value("qos");
    int intervalMs = parser.value("interval-ms").toInt();

    // --- Config & Logging ---
    QString err;
    if (!ConfigManager::ref().load(configFile.isEmpty() ? "config/config.json" : configFile, &err)) {
        qCritical() << "Config load failed:" << err;
        return 1;
    }
    auto& cfg = ConfigManager::ref();
    LogSetup::init(cfg.logging.level, cfg.logging.file);

    // --- Transport: UDP با پورت پویا
    auto* udp = new UdpTransport(/*data port*/ 0, &app);
    ITransport* t = udp;
    qInfo() << "[BOOT][TX] dataPort=" << t->boundPort();

    // --- Core & Ack ---
    AckManager ack;
    DDSCore core(cfg.node_id + "-tx", cfg.protocol_version, t, &ack);

    // --- Discovery از config + لاگ
    const int          discPort = cfg.disc.port;
    const QString      modeStr  = cfg.disc.mode;
    const QHostAddress mcastAdr = cfg.disc.address;
    qInfo() << "[DISC][TX]" << "mode=" << modeStr
            << "grp=" << mcastAdr.toString()
            << "discPort=" << discPort;

    DiscoveryManager disc(cfg.node_id + "-tx", discPort);
    disc.setProtocolVersion(cfg.protocol_version);
    disc.setMode(modeStr);               // اگر enum است، به نسخهٔ enum تبدیل کن
    if (!mcastAdr.isNull()) {
        disc.setMulticastAddress(mcastAdr);
    }
    disc.setDataPort(t->boundPort());
    disc.setAdvertisedTopics(cfg.topics_list.isEmpty() ? QStringList{"sensor/temperature"} : cfg.topics_list);
    QObject::connect(&disc, &DiscoveryManager::peerUpdated, &core, &DDSCore::updatePeers);
    disc.start(/*immediate*/ true);

    // --- Publisher ---
    auto pub = core.makePublisher("sensor/temperature");

    // --- Wait for first peer before sending
    bool firstSent = false;
    QTimer timer(&app);
    timer.setInterval(intervalMs);
    QObject::connect(&timer, &QTimer::timeout, [&](){
        static int i = 0;
        QJsonObject payload{
            { "value", 22 + (i % 5) },
            { "unit",  "C" }
        };
        pub.publish(payload, qos);
        qInfo() << "[TEST][TX]" << "sensor/temperature" << payload;
        ++i;
    });
    QObject::connect(&disc, &DiscoveryManager::peerUpdated, &app, [&](){
        if (firstSent) return;
        firstSent = true;
        QTimer::singleShot(100, [&](){
            QJsonObject firstPayload{{"value", 23}, {"unit","C"}};
            pub.publish(firstPayload, qos);
            qInfo() << "[TEST][TX]" << "sensor/temperature" << firstPayload;
        });
        timer.start();
    });

    return app.exec();
}
