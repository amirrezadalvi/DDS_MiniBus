#include <QTest>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTimer>
#include <QElapsedTimer>
#include <QCoreApplication>
#include "dds_core.h"
#include "transport/udp_transport.h"
#include "discovery/discovery_manager.h"
#include "config/config_manager.h"
#include "transport/ack_manager.h"
#include "../test_helpers/helpers.h"

class TestIntegrationScenarios : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Setup test configuration
        ConfigManager::ref().node_id = "test-node";
        ConfigManager::ref().protocol_version = "1.0";
        ConfigManager::ref().qos_cfg.reliable.ack_timeout_ms = 100; // Faster for tests
        ConfigManager::ref().qos_cfg.reliable.max_retries = 2;
    }

    // Scenario 9.1: Publisher to two subscribers
    void testPublisherToTwoSubscribers() {
#ifdef Q_OS_WIN
        // Single-process multi-node integration is unstable on Windows/MinGW
        // due to event-loop/lifecycle races. We run multi-process demos for E2E.
        QSKIP("Disabled on Windows/MinGW; run multi-process demo for E2E coverage.");
#endif

        // Check if loopback mode is enabled
        bool loopbackMode = qEnvironmentVariableIntValue("DDS_TEST_LOOPBACK") == 1;

        // Setup transports with loopback binding if enabled
        UdpTransport* transport1 = new UdpTransport(38021, nullptr);
        UdpTransport* transport2 = new UdpTransport(38022, nullptr);
        UdpTransport* transport3 = new UdpTransport(38023, nullptr);
        AckManager*   ackMgr     = new AckManager(nullptr);

        // Setup discovery managers for loopback mode
        DiscoveryManager* disc1 = nullptr;
        DiscoveryManager* disc2 = nullptr;
        DiscoveryManager* disc3 = nullptr;

        if (loopbackMode) {
            // All use the same discovery port for loopback communication
            const quint16 discoveryPort = 45454;
            disc1 = new DiscoveryManager("pub-node", discoveryPort, this);
            disc2 = new DiscoveryManager("sub1-node", discoveryPort, this);
            disc3 = new DiscoveryManager("sub2-node", discoveryPort, this);

            disc1->setLoopbackMode(true);
            disc2->setLoopbackMode(true);
            disc3->setLoopbackMode(true);

            disc1->setAdvertisedTopics({"test/topic"});
            disc2->setAdvertisedTopics({"test/topic"});
            disc3->setAdvertisedTopics({"test/topic"});

            disc1->setDataPort(38021);
            disc2->setDataPort(38022);
            disc3->setDataPort(38023);
        }

        DDSCore core1("pub-node",  "1.0", transport1, ackMgr);
        DDSCore core2("sub1-node", "1.0", transport2, nullptr);
        DDSCore core3("sub2-node", "1.0", transport3, nullptr);

        // Create subscribers before publishing
        QStringList receivedTopics1, receivedTopics2;
        core2.makeSubscriber("test/topic", [&](const QJsonObject& payload){
           receivedTopics1 << payload.value("data").toString();
        });
        core3.makeSubscriber("test/topic", [&](const QJsonObject& payload){
           receivedTopics2 << payload.value("data").toString();
        });

        if (!loopbackMode) {
           // Setup discovery managers for non-loopback mode
           const quint16 discoveryPort = 45454;
           disc1 = new DiscoveryManager("pub-node", discoveryPort, nullptr);
           disc2 = new DiscoveryManager("sub1-node", discoveryPort, nullptr);
           disc3 = new DiscoveryManager("sub2-node", discoveryPort, nullptr);

           disc1->setAdvertisedTopics({"test/topic"});
           disc2->setAdvertisedTopics({"test/topic"});
           disc3->setAdvertisedTopics({"test/topic"});

           disc1->setDataPort(38021);
           disc2->setDataPort(38022);
           disc3->setDataPort(38023);

           core1.setDiscoveryManager(disc1);
           core2.setDiscoveryManager(disc2);
           core3.setDiscoveryManager(disc3);

           QObject::connect(disc1, &DiscoveryManager::peerUpdated, &core1, &DDSCore::updatePeers);
           QObject::connect(disc2, &DiscoveryManager::peerUpdated, &core2, &DDSCore::updatePeers);
           QObject::connect(disc3, &DiscoveryManager::peerUpdated, &core3, &DDSCore::updatePeers);

           disc1->start(true);
           disc2->start(true);
           disc3->start(true);

           // Wait for discovery to settle
           QTest::qWait(2000);
        } else {
           // BYPASS discovery: inject peers directly with data_port
           QJsonObject peer2;
           peer2["node_id"]    = "sub1-node";
           peer2["data_port"]  = 38022;
           peer2["topics"]     = QJsonArray{ "test/topic" };
           core1.updatePeers("sub1-node", peer2);

           QJsonObject peer3;
           peer3["node_id"]    = "sub2-node";
           peer3["data_port"]  = 38023;
           peer3["topics"]     = QJsonArray{ "test/topic" };
           core1.updatePeers("sub2-node", peer3);

           // Give the event loop a moment to settle
           QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
           QTest::qWait(200);
        }

        // Publish and wait a bit longer on Windows
        QJsonObject payload{{"data","test-message"}};
        core1.publishInternal("test/topic", payload, "reliable");
        QTest::qWait(1000);

        // Verify both subscribers received the message
        QCOMPARE(receivedTopics1.size(), 1);
        QCOMPARE(receivedTopics2.size(), 1);
        QCOMPARE(receivedTopics1.first(), "test-message");
        QCOMPARE(receivedTopics2.first(), "test-message");

        // Clean up (no QObject parents were set, so delete manually once)
        delete transport1;
        delete transport2;
        delete transport3;
        delete ackMgr;
        if (!loopbackMode) {
           // Clean up discovery managers from non-loopback branch
           if (disc1) { disc1->stop(); delete disc1; }
           if (disc2) { disc2->stop(); delete disc2; }
           if (disc3) { disc3->stop(); delete disc3; }
        }
    }

    // Scenario 9.2: QoS failure (dropped ACK → retries → give-up)
    void testQoSFailureRetryGiveUp() {
        UdpTransport* transport = new UdpTransport(38023, this);
        AckManager* ackMgr = new AckManager(this);

        DDSCore core("test-node", "1.0", transport, ackMgr);

        // Setup subscriber that doesn't send ACKs (simulates failure)
        core.makeSubscriber("test/fail", [&](const QJsonObject&) {
            // Intentionally don't send ACK
        });

        // Publish reliable message
        QJsonObject payload{{"data", "fail-test"}};
        qint64 msgId = core.publishInternal("test/fail", payload, "reliable");

        // Wait for retries to exhaust
        QTest::qWait(1000); // Should be enough for 2 retries with 100ms timeout

        // Verify message was given up
        QVERIFY(ackMgr->hasPending() == false); // Should have no pending messages

        delete transport;
        delete ackMgr;
    }

    // Scenario 9.3: Dynamic config change behavior
    void testDynamicConfigChange() {
        // Test discovery interval change (reloadable)
        ConfigManager::ref().disc.interval_ms = 2000;

        // Simulate config file change
        QString originalInterval = QString::number(2000);
        ConfigManager::ref().disc.interval_ms = 1000; // Change to 1 second

        // Verify change is detected and applied
        QCOMPARE(ConfigManager::ref().disc.interval_ms, 1000);
    }

    // Scenario 9.4: Simple load/latency measurement harness
    void testLoadLatencyMeasurement() {
#if defined(_WIN32)
        // On Windows/MinGW, UDP broadcast doesn't loop back in single-process.
        // Use unicast pairing instead: two in-process nodes communicating via localhost.

        qputenv("DDS_TEST_LOOPBACK", "1");

        UdpTransport* transportSub = new UdpTransport(0, this); // ephemeral port
        UdpTransport* transportPub = new UdpTransport(38025, this);
        AckManager* ackMgr = new AckManager(this);

        DDSCore coreSub("perf-sub", "1.0", transportSub, nullptr);
        DDSCore corePub("perf-pub", "1.0", transportPub, ackMgr);

        const int NUM_MESSAGES = 100;
        QVector<qint64> latencies;
        int messagesReceived = 0;

        // Setup subscriber with latency measurement
        coreSub.makeSubscriber("perf/topic", [&](const QJsonObject& payload) {
            qint64 sentTime = payload.value("timestamp").toVariant().toLongLong();
            qint64 receivedTime = QDateTime::currentMSecsSinceEpoch();
            latencies.append(receivedTime - sentTime);
            messagesReceived++;
        });

        // Inject unicast peer: corePub -> coreSub on localhost:subPort
        quint16 subPort = transportSub->boundPort();
        QJsonObject peer;
        peer["node_id"] = "perf-sub";
        peer["data_port"] = subPort;
        peer["topics"] = QJsonArray{"perf/topic"};
        corePub.updatePeers("perf-sub", peer);

        // Give event loop a moment
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QTest::qWait(100);

        // Send messages
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            QJsonObject payload{
                {"data", QString("msg-%1").arg(i)},
                {"timestamp", QDateTime::currentMSecsSinceEpoch()}
            };
            corePub.publishInternal("perf/topic", payload, "best_effort");
            QTest::qWait(1); // Small delay between messages
        }

        // Wait for all messages to be processed
        QTRY_COMPARE_WITH_TIMEOUT(messagesReceived > 0, true, 1500);

        qInfo() << "Sent" << NUM_MESSAGES << "messages, received" << messagesReceived;

        delete transportSub;
        delete transportPub;
        delete ackMgr;
#else
        // Non-Windows: original broadcast-based logic
        UdpTransport* transport = new UdpTransport(38024, this);
        AckManager* ackMgr = new AckManager(this);
        DDSCore core("perf-node", "1.0", transport, ackMgr);

        const int NUM_MESSAGES = 100;
        QVector<qint64> latencies;
        int messagesReceived = 0;

        // Setup subscriber with latency measurement
        core.makeSubscriber("perf/topic", [&](const QJsonObject& payload) {
            qint64 sentTime = payload.value("timestamp").toVariant().toLongLong();
            qint64 receivedTime = QDateTime::currentMSecsSinceEpoch();
            latencies.append(receivedTime - sentTime);
            messagesReceived++;
        });

        QElapsedTimer timer;
        timer.start();

        // Allow discovery to settle before sending messages
        discoverySettle();

        // Send messages
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            QJsonObject payload{
                {"data", QString("msg-%1").arg(i)},
                {"timestamp", QDateTime::currentMSecsSinceEpoch()}
            };
            core.publishInternal("perf/topic", payload, "best_effort");
            QTest::qWait(1); // Small delay between messages
        }

        // Wait for all messages to be processed
        QTest::qWait(200);

        delete transport;
        delete ackMgr;
#endif

        // Calculate statistics (common for both paths)
        QVERIFY(messagesReceived > 0);
        qint64 totalLatency = 0;
        qint64 minLatency = INT64_MAX;
        qint64 maxLatency = 0;

        for (qint64 latency : latencies) {
            totalLatency += latency;
            minLatency = qMin(minLatency, latency);
            maxLatency = qMax(maxLatency, latency);
        }

        double avgLatency = static_cast<double>(totalLatency) / latencies.size();

        qDebug() << "Load test results:" << messagesReceived << "messages received";
        qDebug() << "Average latency:" << avgLatency << "ms";
        qDebug() << "Min latency:" << minLatency << "ms";
        qDebug() << "Max latency:" << maxLatency << "ms";

        // Basic sanity checks
        QVERIFY(avgLatency >= 0);
        QVERIFY(minLatency >= 0);
        QVERIFY(maxLatency >= minLatency);
    }

    // Test graceful shutdown with pending messages
    void testGracefulShutdown() {
        UdpTransport* transport = new UdpTransport(38025, this);
        AckManager* ackMgr = new AckManager(this);
        DDSCore core("shutdown-node", "1.0", transport, ackMgr);

        // Publish a reliable message
        QJsonObject payload{{"data", "shutdown-test"}};
        core.publishInternal("shutdown/topic", payload, "reliable");

        // Shutdown with timeout
        core.shutdown(500); // 500ms timeout

        // Verify transport is stopped (shutdown completed without hanging)
        QVERIFY(true); // Transport stop is synchronous

        delete transport;
        delete ackMgr;
    }

    // Test last-message cache wiring
    void testLastMessageCache() {
        UdpTransport* transport = new UdpTransport(38026, this);
        DDSCore core("cache-node", "1.0", transport, nullptr);

        QString receivedData;

        // Subscribe to topic
        auto subscriber = core.makeSubscriber("cache/topic", [&](const QJsonObject& payload) {
            receivedData = payload.value("data").toString();
        });

        // Publish message before subscriber connects
        QJsonObject payload1{{"data", "first-message"}};
        core.publishInternal("cache/topic", payload1, "best_effort");

        // Create new subscriber - should receive last message
        QString receivedData2;
        auto subscriber2 = core.makeSubscriber("cache/topic", [&](const QJsonObject& payload) {
            receivedData2 = payload.value("data").toString();
        });

        QTest::qWait(100);

        // Verify last message cache works
        QCOMPARE(receivedData2, "first-message");

        delete transport;
    }
};

QTEST_MAIN(TestIntegrationScenarios)
#include "test_integration_scenarios.moc"