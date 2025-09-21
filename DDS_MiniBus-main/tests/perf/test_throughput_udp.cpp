#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <iostream>
#include "dds_core.h"
#include "publisher.h"
#include "udp_transport.h"
#include "ack_manager.h"
#include "discovery_manager.h"
#include "config_manager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Config
    QString configPath = "config/config.json";
    QString err;
    if (!ConfigManager::ref().load(configPath, &err)) {
        qCritical() << "Config load failed:" << err;
        return 1;
    }
    auto& cfg = ConfigManager::ref();

    // Transport
    ITransport* transport = new UdpTransport(cfg.transport.udp.port, &app);
    AckManager ack(&app);
    DDSCore core(cfg.node_id, cfg.protocol_version, transport, &ack);

    // Publisher
    Publisher pub = core.makePublisher("sensor/temperature");

    // Test params
    int numMessages = 50000;
    int sent = 0;
    QElapsedTimer timer;
    timer.start();

    QTimer* sendTimer = new QTimer(&app);
    QObject::connect(sendTimer, &QTimer::timeout, [&]() {
        if (sent >= numMessages) {
            sendTimer->stop();
            qint64 elapsed = timer.elapsed();
            double msgsPerSec = (double)numMessages / (elapsed / 1000.0);
            qInfo() << "Throughput:" << msgsPerSec << "msgs/sec";
            app.quit();
            return;
        }
        QJsonObject payload{{"value", 23.5}, {"unit", "C"}};
        pub.publish(payload, "best_effort");
        sent++;
    });

    sendTimer->start(0); // as fast as possible

    return app.exec();
}