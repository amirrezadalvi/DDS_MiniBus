#include <QtTest/QtTest>
#include "../common/test_utils.h"

class TestQosFailure : public QObject {
    Q_OBJECT

private slots:
    void testQosFailureAndDeadLetter() {
        // Skip if multicast is disabled in this environment
        if (qEnvironmentVariableIsEmpty("ALLOW_MULTICAST_TESTS")) {
            QSKIP("Multicast disabled in this environment (set ALLOW_MULTICAST_TESTS=1 to enable).");
        }

        // Start subscriber (but we'll simulate ACK failure)
        TestProcess sub;
        QStringList subArgs = {
            "--role", "subscriber",
            "--topic", "sensor/temperature",
            "--config", IntegrationTestUtils::getTestConfigPath("config_rx.json"),
            "--log-level", "info"
        };
        QVERIFY(sub.start(IntegrationTestUtils::getTestBinaryPath(), subArgs));

        // Wait for subscriber to start
        QTest::qWait(1000);

        // Start publisher with reliable QoS
        TestProcess pub;
        QStringList pubArgs = {
            "--role", "sender",
            "--topic", "sensor/temperature",
            "--qos", "reliable",
            "--count", "1",
            "--interval-ms", "100",
            "--payload", R"({"value": 23.5, "unit": "C"})",
            "--config", IntegrationTestUtils::getTestConfigPath("config_tx.json"),
            "--log-level", "debug"
        };
        QVERIFY(pub.start(IntegrationTestUtils::getTestBinaryPath(), pubArgs));

        // Wait for publisher to finish (should timeout and give up)
        QVERIFY(pub.waitForFinished(10000)); // Allow time for retries

        // Check results
        auto pubResult = pub.result();
        auto subResult = sub.result();

        // Parse logs
        auto sentMessages = pub.parseSentMessages();
        auto resendLines = pub.findLines(QRegularExpression("\\[RESEND\\]"));
        auto deadLetterLines = pub.findLines(QRegularExpression("\\[DEADLETTER\\]"));

        qDebug() << "Sent messages:" << sentMessages.size();
        qDebug() << "Resend attempts:" << resendLines.size();
        qDebug() << "Dead letter events:" << deadLetterLines.size();

        // Assertions
        QCOMPARE(sentMessages.size(), 1); // One message sent

        // Should have retries (max_retries = 3 from config)
        QVERIFY(resendLines.size() >= 3); // At least max_retries attempts

        // Should have dead letter entry
        QVERIFY(deadLetterLines.size() >= 1);

        // Clean up
        sub.terminateProcess();
        sub.waitForFinished(3000);

        // Print summary
        qInfo() << "=== QoS Failure Test Summary ===";
        qInfo() << "Messages sent:" << sentMessages.size();
        qInfo() << "Resend attempts:" << resendLines.size();
        qInfo() << "Dead letter events:" << deadLetterLines.size();
        qInfo() << "Test PASSED";
    }
};

QTEST_MAIN(TestQosFailure)
#include "test_qos_failure.moc"