#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include "ack_manager.h"
#include "dds_core.h"
#include "publisher.h"
#include "config_manager.h"

class TestAckManager : public QObject {
    Q_OBJECT

private slots:
    void testRetriesThenGiveup() {
        AckManager ack;
        QSignalSpy resendSpy(&ack, &AckManager::resend);
        QSignalSpy failedSpy(&ack, &AckManager::failed);

        qint64 msgId = 42;
        QString receiverId = "peer-1";
        Pending p;
        p.packet = "dummy";
        p.retries_left = 2;
        p.deadline_ms = QDateTime::currentMSecsSinceEpoch() + 10;
        p.base_timeout_ms = 10;
        p.attempt = 0;
        p.exponential_backoff = false;
        p.msg_id = msgId;
        p.receiver_id = receiverId;

        ack.track(p);

        // Wait for retries and giveup
        QTRY_COMPARE_WITH_TIMEOUT(resendSpy.count(), 2, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 1000);

        // Check failed signal args
        QCOMPARE(failedSpy.at(0).at(0).toLongLong(), msgId);
        QCOMPARE(failedSpy.at(0).at(1).toString(), receiverId);
    }

    void testAckBeforeGiveup() {
        AckManager ack;
        QSignalSpy resendSpy(&ack, &AckManager::resend);
        QSignalSpy failedSpy(&ack, &AckManager::failed);

        qint64 msgId = 43;
        QString receiverId = "peer-2";
        Pending p;
        p.packet = "dummy";
        p.retries_left = 2;
        p.deadline_ms = QDateTime::currentMSecsSinceEpoch() + 10;
        p.base_timeout_ms = 10;
        p.attempt = 0;
        p.exponential_backoff = false;
        p.msg_id = msgId;
        p.receiver_id = receiverId;

        ack.track(p);

        // Simulate ACK received
        ack.ackReceived(msgId, receiverId);

        // Wait a bit, ensure no failed
        QTest::qWait(50);
        QCOMPARE(failedSpy.count(), 0);
        // Resend might have happened once before ACK, but no failed
    }

    void testDeadLetterBufferBounded() {
        AckManager ack;
        QSignalSpy failedSpy(&ack, &AckManager::failed);

        // Fill with 130 failed messages
        for (int i = 0; i < 130; ++i) {
            Pending p;
            p.packet = "dummy";
            p.retries_left = 0;
            p.deadline_ms = QDateTime::currentMSecsSinceEpoch() - 1;
            p.base_timeout_ms = 1;
            p.attempt = 0;
            p.exponential_backoff = false;
            p.msg_id = 1000 + i;
            p.receiver_id = QString("peer-%1").arg(i);
            ack.track(p);
        }
        // Wait for all to fail
        QTest::qWait(100);
        // Dead letter buffer should be capped at 128
        QCOMPARE(ack.deadLetterSize(), 128);
    }

    void testExponentialBackoff() {
        AckManager ack;
        QSignalSpy resendSpy(&ack, &AckManager::resend);

        const int base = 120; // Use realistic timeout for stability
        const int maxr = 2;
        const int total = base * ((1<<maxr) - 1); // base * (2^maxr - 1)
        const int budget = int(std::ceil(total * 1.8)) + 200; // jitter guard

        Pending p;
        p.packet = "dummy";
        p.retries_left = maxr;
        p.deadline_ms = QDateTime::currentMSecsSinceEpoch() + base;
        p.base_timeout_ms = base;
        p.attempt = 0;
        p.exponential_backoff = true;
        p.msg_id = 555;
        p.receiver_id = "peer-exp";
        ack.track(p);

        // Wait for all retries to complete
        QTRY_COMPARE_WITH_TIMEOUT(resendSpy.count(), maxr, budget);
    }

    void testDeadLetter() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString deadletterPath = tempDir.filePath("deadletter.ndjson");

        // Create a custom config with deadletter file
        ConfigManager& cfg = ConfigManager::ref();
        cfg.logging.deadletter_file = deadletterPath;
        cfg.qos_cfg.reliable.ack_timeout_ms = 80;
        cfg.qos_cfg.reliable.max_retries = 2;

        AckManager ack;
        QSignalSpy dlSpy(&ack, &AckManager::deadLetter);

        qint64 msgId = 999;
        QString receiverId = "test-peer";

        Pending p;
        p.packet = "dummy";
        p.retries_left = 2;
        p.deadline_ms = QDateTime::currentMSecsSinceEpoch() + 80;
        p.base_timeout_ms = 80;
        p.attempt = 0;
        p.exponential_backoff = false;
        p.msg_id = msgId;
        p.receiver_id = receiverId;

        ack.track(p);

        // Wait for dead letter (budget: 80 * (2^2 - 1) * 1.8 + 200 = ~500ms)
        const int budget = int(std::ceil(80 * 3 * 1.8)) + 200;
        QTRY_VERIFY_WITH_TIMEOUT(dlSpy.count() >= 1, budget);

        // Check signal args
        QCOMPARE(dlSpy.at(0).at(0).toLongLong(), msgId);
        QCOMPARE(dlSpy.at(0).at(1).toString(), receiverId);
        QCOMPARE(dlSpy.at(0).at(2).toInt(), 2); // attempts

        // Check NDJSON file
        QFile f(deadletterPath);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QByteArray data = f.readAll();
        f.close();

        // Parse last line as JSON
        QStringList lines = QString(data).split('\n', Qt::SkipEmptyParts);
        QVERIFY(!lines.isEmpty());

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(lines.last().toUtf8(), &err);
        QVERIFY(err.error == QJsonParseError::NoError);
        QVERIFY(doc.isObject());

        QJsonObject obj = doc.object();
        QCOMPARE(obj.value("message_id").toVariant().toLongLong(), msgId);
        QCOMPARE(obj.value("receiver").toString(), receiverId);
        QCOMPARE(obj.value("attempts").toInt(), 2);
        QCOMPARE(obj.value("reason").toString(), QString("max_retries_exceeded"));
        QVERIFY(obj.contains("ts"));
    }

    void testRetainLast() {
        // Create DDSCore with retain_last enabled
        ConfigManager& cfg = ConfigManager::ref();
        cfg.qos_cfg.retain_last = true;
        cfg.node_id = "test-node";

        // Mock transport (we don't need real network for this test)
        class MockTransport : public ITransport {
        public:
            MockTransport() : ITransport(nullptr) {}
            bool send(const QByteArray& datagram, const QHostAddress& to, quint16 port) override {
                Q_UNUSED(datagram); Q_UNUSED(to); Q_UNUSED(port);
                return true;
            }
            quint16 boundPort() const override { return 12345; }
            void stop() override {}
        };

        MockTransport transport;
        AckManager ack;
        DDSCore core("test-node", "1.0", &transport, &ack);

        bool messageReceived = false;
        QString receivedTopic;
        QJsonObject receivedPayload;

        // Subscribe first (should get retained message)
        core.makeSubscriber("sensor/temperature", [&](const QJsonObject& payload) {
            messageReceived = true;
            receivedTopic = payload.value("topic").toString();
            receivedPayload = payload;
        });

        // Publish a message
        auto pub = core.makePublisher("sensor/temperature");
        QJsonObject testPayload{{"value", 42.0}, {"unit", "C"}};
        pub.publish(testPayload, "best_effort");

        // Subscribe again (should immediately receive retained message)
        messageReceived = false;
        core.makeSubscriber("sensor/temperature", [&](const QJsonObject& payload) {
            messageReceived = true;
            receivedTopic = payload.value("topic").toString();
            receivedPayload = payload;
        });

        // Should have received the retained message
        QVERIFY(messageReceived);
        QCOMPARE(receivedTopic, QString("sensor/temperature"));
        QCOMPARE(receivedPayload.value("value").toDouble(), 42.0);
        QCOMPARE(receivedPayload.value("unit").toString(), QString("C"));
    }
};

QTEST_MAIN(TestAckManager)
#include "test_ack_manager.moc"