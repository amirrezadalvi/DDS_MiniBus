#include <QtTest/QtTest>
#include "../common/test_utils.h"
#include "../test_helpers/helpers.h"

class TestDiscoveryCycle : public QObject {
    Q_OBJECT

private slots:
    void testDiscoveryAndExpiry() {
        // Skip if multicast is disabled in this environment
        if (qEnvironmentVariableIsEmpty("ALLOW_MULTICAST_TESTS")) {
            QSKIP("Multicast disabled in this environment (set ALLOW_MULTICAST_TESTS=1 to enable).");
        }

        // Use absolute config paths for robustness
        const QString configRxPath = getConfigPath("config_rx.json");
        const QString configRx2Path = getConfigPath("config_rx2.json");

        // Start node A
        TestProcess nodeA;
        QStringList nodeAArgs = {
            "--role", "subscriber",
            "--topic", "test/topic",
            "--config", configRxPath,
            "--log-level", "info",
            "--run-for-sec", "5"
        };
        QVERIFY(nodeA.start(IntegrationTestUtils::getTestBinaryPath(), nodeAArgs));

        // Start node B with different config
        TestProcess nodeB;
        QStringList nodeBArgs = {
            "--role", "subscriber",
            "--topic", "test/topic",
            "--config", configRx2Path,
            "--log-level", "info",
            "--run-for-sec", "5"
        };
        QVERIFY(nodeB.start(IntegrationTestUtils::getTestBinaryPath(), nodeBArgs));

        // Discovery settlement delay (Windows: 2500ms, Other: 1000ms)
        discoverySettle();

        // Check that both nodes discovered each other
        auto nodeALines = nodeA.findLines(QRegularExpression("discovery: peer="));
        auto nodeBLines = nodeB.findLines(QRegularExpression("discovery: peer="));

        QVERIFY(nodeALines.size() >= 1); // Should see node B
        QVERIFY(nodeBLines.size() >= 1); // Should see node A

        qDebug() << "Node A discovered peers:" << nodeALines.size();
        qDebug() << "Node B discovered peers:" << nodeBLines.size();

        // Stop node A (it should exit on its own after --run-for-sec 5)
        QVERIFY(nodeA.waitForFinished(10000));

        // Wait for expiry (3 intervals = ~3 seconds)
        QTest::qWait(3500);

        // Check that node B expired node A
        auto expiryLines = nodeB.findLines(QRegularExpression("discovery: expired peer="));
        QVERIFY(expiryLines.size() >= 1);

        qDebug() << "Expiry events:" << expiryLines.size();

        // Node B should also exit on its own
        QVERIFY(nodeB.waitForFinished(10000));

        // Print summary
        qInfo() << "=== Discovery Test Summary ===";
        qInfo() << "Node A peers discovered:" << nodeALines.size();
        qInfo() << "Node B peers discovered:" << nodeBLines.size();
        qInfo() << "Expiry events:" << expiryLines.size();
        qInfo() << "Test PASSED";
    }
};

QTEST_MAIN(TestDiscoveryCycle)
#include "test_discovery_cycle.moc"