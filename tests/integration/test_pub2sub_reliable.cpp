#include <QtTest/QtTest>
#include "../common/test_utils.h"
#include "../test_helpers/helpers.h"
#include <QVector>
#include <QSet>

class TestPub2SubReliable : public QObject {
    Q_OBJECT

private slots:
    void testReliableDeliveryToTwoSubscribers() {
        // Skip if multicast is disabled in this environment
        if (qEnvironmentVariableIsEmpty("ALLOW_MULTICAST_TESTS")) {
            QSKIP("Multicast disabled in this environment (set ALLOW_MULTICAST_TESTS=1 to enable).");
        }

        // Setup
        const QString topic = "sensor/temperature";
        const int messageCount = 3;
        const int intervalMs = 200;
        const QString payload = R"({"value": 23.5, "unit": "C"})";

        // Use absolute config paths for robustness
        const QString configRxPath = getConfigPath("config_rx.json");
        const QString configRx2Path = getConfigPath("config_rx2.json");
        const QString configTxPath = getConfigPath("config_tx.json");

        // Log current working directory for debugging
        qDebug() << "[TEST] Current working directory:" << QDir::currentPath();
        qDebug() << "[TEST] Config RX path:" << configRxPath;
        qDebug() << "[TEST] Config RX2 path:" << configRx2Path;
        qDebug() << "[TEST] Config TX path:" << configTxPath;

        // Start subscriber 1
        TestProcess sub1;
        QStringList sub1Args = {
            "--role", "subscriber",
            "--topic", topic,
            "--config", configRxPath,
            "--log-level", "info",
            "--print-recv",
            "--run-for-sec", "5"
        };
        QVERIFY(sub1.start(IntegrationTestUtils::getTestBinaryPath(), sub1Args));

        // Start subscriber 2 with different config
        TestProcess sub2;
        QStringList sub2Args = {
            "--role", "subscriber",
            "--topic", topic,
            "--config", configRx2Path,
            "--log-level", "info",
            "--print-recv",
            "--run-for-sec", "5"
        };
        QVERIFY(sub2.start(IntegrationTestUtils::getTestBinaryPath(), sub2Args));

        // Discovery warm-up: wait for subscribers to be discovered
        discoverySettle(); // Windows: 2500ms, Other: 1000ms

        // Start publisher with delay to let discovery settle
        TestProcess pub;
        QStringList pubArgs = {
            "--role", "sender",
            "--topic", topic,
            "--qos", "reliable",
            "--count", QString::number(messageCount),
            "--interval-ms", QString::number(intervalMs),
            "--payload", payload,
            "--config", configTxPath,
            "--log-level", "debug",
            "--start-delay-ms", "1500",
            "--run-for-sec", "5"
        };
        QVERIFY(pub.start(IntegrationTestUtils::getTestBinaryPath(), pubArgs));

        // All processes should exit on their own after --run-for-sec 5
        QVERIFY(pub.waitForFinished(20000)); // sender exits cleanly
        QVERIFY(sub1.waitForFinished(10000)); // subscriber 1 exits cleanly
        QVERIFY(sub2.waitForFinished(10000)); // subscriber 2 exits cleanly

        // Check publisher exited cleanly (robust even if logs go to file)
        QCOMPARE(pub.exitStatus(), QProcess::NormalExit);
        QCOMPARE(pub.exitCode(), 0);

        // On failure, dump log tail for diagnosis
        if (pub.exitStatus() != QProcess::NormalExit || pub.exitCode() != 0) {
            const QString logPath = QDir::current().absoluteFilePath("logs/dds.log");
            QFile f(logPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QByteArray data = f.readAll();
                const int maxTail = 4096;
                if (data.size() > maxTail) data = data.right(maxTail);
                qDebug() << "[LOG TAIL]\n" << QString::fromUtf8(data);
            } else {
                qDebug() << "[LOG TAIL] cannot open" << logPath;
            }
        }

        // Use robust output checking with timeouts
        QVERIFY2(waitForContains(sub1, "RECV topic=sensor/temperature", 5000),
                 "Subscriber #1 did not print RECV line in time");
        QVERIFY2(waitForContains(sub2, "RECV topic=sensor/temperature", 5000),
                 "Subscriber #2 did not print RECV line in time");

        // Collect final outputs for detailed analysis
        const QString outS1 = sub1.readAllStdout();
        const QString outS2 = sub2.readAllStdout();

        // Count RECV lines in subscriber outputs (more lenient for Windows timing)
        auto countOcc = [](const QString& s, const QString& needle) {
            int c = 0, pos = 0;
            while ((pos = s.indexOf(needle, pos, Qt::CaseInsensitive)) != -1) {
                c++;
                pos += needle.size();
            }
            return c;
        };

        const int n1 = countOcc(outS1, "RECV ");
        const int n2 = countOcc(outS2, "RECV ");
        QVERIFY2(n1 >= 1, QString("Subscriber #1 did not receive any messages (got %1)").arg(n1).toUtf8());
        QVERIFY2(n2 >= 1, QString("Subscriber #2 did not receive any messages (got %1)").arg(n2).toUtf8());

        // (Optional) Check log file for completion markers if logging to file
        {
          const QString logPath = QDir::current().absoluteFilePath("logs/dds.log");
          QFile f(logPath);
          if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            const QString logs = QString::fromUtf8(f.readAll());
            QVERIFY2(logs.contains("sender completed") || logs.contains("cli:"),
                     "No publisher completion markers found in dds.log");
          }
        }

        // Parse results for detailed analysis
        auto pubResult = pub.result();
        auto sub1Result = sub1.result();
        auto sub2Result = sub2.result();

        auto sentMessages = pub.parseSentMessages();
        auto sub1Messages = sub1.parseReceivedMessages();
        auto sub2Messages = sub2.parseReceivedMessages();

        // Assertions
        QCOMPARE(sentMessages.size(), messageCount);
        QCOMPARE(sub1Messages.size(), messageCount);
        QCOMPARE(sub2Messages.size(), messageCount);

        // Check for duplicates
        QSet<qint64> sub1Ids, sub2Ids;
        for (const auto& msg : sub1Messages) sub1Ids.insert(msg.messageId);
        for (const auto& msg : sub2Messages) sub2Ids.insert(msg.messageId);

        QCOMPARE(sub1Ids.size(), messageCount); // No duplicates
        QCOMPARE(sub2Ids.size(), messageCount); // No duplicates

        // Check ACKs and resends
        int ackCount = pub.countAcks();
        int resendCount = pub.countResends();

        qDebug() << "ACK count:" << ackCount << "Resend count:" << resendCount;

        // Should have ACKs from both subscribers for each message
        QVERIFY(ackCount >= messageCount * 2); // At least 2 ACKs per message

        // Calculate latencies
        qint64 totalLatency = 0;
        qint64 maxLatency = 0;
        int latencyCount = 0;

        for (const auto& sent : sentMessages) {
            // Find corresponding received messages
            for (const auto& recv : sub1Messages) {
                if (recv.messageId == sent.messageId) {
                    qint64 latency = IntegrationTestUtils::calculateLatency(sent, recv);
                    totalLatency += latency;
                    maxLatency = qMax(maxLatency, latency);
                    latencyCount++;
                    break;
                }
            }
            for (const auto& recv : sub2Messages) {
                if (recv.messageId == sent.messageId) {
                    qint64 latency = IntegrationTestUtils::calculateLatency(sent, recv);
                    totalLatency += latency;
                    maxLatency = qMax(maxLatency, latency);
                    latencyCount++;
                    break;
                }
            }
        }

        if (latencyCount > 0) {
            qint64 avgLatency = totalLatency / latencyCount;
            qDebug() << "Average latency:" << avgLatency << "ms, Max latency:" << maxLatency << "ms";

            // Assert latency budget (300ms is generous for local testing)
            QVERIFY(avgLatency < 300);
            QVERIFY(maxLatency < 500);
        }

        // Print summary
        qInfo() << "=== Integration Test Summary ===";
        qInfo() << "Messages sent:" << sentMessages.size();
        qInfo() << "Messages received (sub1):" << sub1Messages.size();
        qInfo() << "Messages received (sub2):" << sub2Messages.size();
        qInfo() << "ACKs received:" << ackCount;
        qInfo() << "Resends:" << resendCount;
        qInfo() << "Duplicates detected: 0";
        qInfo() << "Test PASSED";
    }
};

QTEST_MAIN(TestPub2SubReliable)
#include "test_pub2sub_reliable.moc"