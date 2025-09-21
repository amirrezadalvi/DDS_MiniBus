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

    // --- Transport: UDP با پورت پویا برای جلوگیری از تداخل
    auto* udp = new UdpTransport(/*data port*/ 0, &app);
    ITransport* t = udp;
    qInfo() << "[BOOT][RX] dataPort=" << t->boundPort();

    // --- Core & Ack ---
    AckManager ack;
    DDSCore core(cfg.node_id + "-rx", cfg.protocol_version, t, &ack);

    // --- Discovery از config (دقت: disc.*)
    const int          discPort = cfg.disc.port;     // مثل 45454
    const QString      modeStr  = cfg.disc.mode;     // "multicast" یا "broadcast"
    const QHostAddress mcastAdr = cfg.disc.address;  // 239.255.0.1
    qInfo() << "[DISC][RX]" << "mode=" << modeStr
            << "grp=" << mcastAdr.toString()
            << "discPort=" << discPort;

    DiscoveryManager disc(cfg.node_id + "-rx", discPort);
    disc.setProtocolVersion(cfg.protocol_version);
    disc.setMode(modeStr);               // اگر امضایش enum است، این خط را طبق enum خودت تغییر بده
    if (!mcastAdr.isNull()) {
        disc.setMulticastAddress(mcastAdr);
    }
    disc.setDataPort(t->boundPort());
    disc.setAdvertisedTopics(cfg.topics_list.isEmpty() ? QStringList{"sensor/temperature"} : cfg.topics_list);
    QObject::connect(&disc, &DiscoveryManager::peerUpdated, &core, &DDSCore::updatePeers);
    disc.start(/*immediate*/ true);

    // --- Subscriber نمونه
    core.makeSubscriber("sensor/temperature", [](const QJsonObject& pl){
        qInfo() << "[TEST][RX] temperature:" << pl;
    });

    return app.exec();
}
