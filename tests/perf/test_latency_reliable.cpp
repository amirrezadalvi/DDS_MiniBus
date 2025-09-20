#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <iostream>
#include <vector>
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
    int numMessages = 5000;
    int sent = 0;
    std::vector<qint64> latencies;

    QTimer* sendTimer = new QTimer(&app);
    QObject::connect(sendTimer, &QTimer::timeout, [&]() {
        if (sent >= numMessages) {
            sendTimer->stop();
            // Calculate stats
            if (!latencies.empty()) {
                std::sort(latencies.begin(), latencies.end());
                double mean = 0;
                for (auto l : latencies) mean += l;
                mean /= latencies.size();
                double p95 = latencies[latencies.size() * 0.95];
                qInfo() << "Latency mean:" << mean << "ms, 95p:" << p95 << "ms";
            }
            app.quit();
            return;
        }
        QJsonObject payload{{"value", 23.5}, {"unit", "C"}};
        QElapsedTimer t;
        t.start();
        qint64 msgId = pub.publish(payload, "reliable");
        // Simulate wait for ACK, but since no receiver, just record send time
        latencies.push_back(t.elapsed()); // approximate
        sent++;
    });

    sendTimer->start(10); // 10ms interval

    return app.exec();
}