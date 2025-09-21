/* tests/common/test_utils.h */
#pragma once
#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QStringList>
#include <QRegularExpression>
#include <QVector>

struct ProcessResult {
    int exitCode;
    QStringList stdoutLines;
    QStringList stderrLines;
    qint64 elapsedMs;
};

struct MessageRecord {
    qint64 messageId;
    qint64 timestamp;
    QString payload;
};

class TestProcess : public QObject {
  Q_OBJECT
public:
  explicit TestProcess(QObject* parent=nullptr);
  bool start(const QString& program, const QStringList& arguments);
  bool waitForFinished(int msec);
  QString readAllStdout() const;
  QString readAllStderr() const;
  void terminate();
  bool terminateProcess(int timeoutMs = 3000);   // <— NEW

  // NEW accessors
  int exitCode() const;
  QProcess::ExitStatus exitStatus() const;
  bool isRunning() const;

  // Additional methods for integration tests
  ProcessResult result() const;
  QStringList findLines(const QRegularExpression& pattern) const;
  QVector<MessageRecord> parseSentMessages() const;
  QVector<MessageRecord> parseReceivedMessages() const;
  int countAcks() const;
  int countResends() const;

private slots:
  void onReadyReadStandardOutput();
  void onReadyReadStandardError();

private:
  QProcess* process = nullptr;
  QElapsedTimer timer;
  QStringList stdoutBuffer;
  QStringList stderrBuffer;
};

namespace IntegrationTestUtils {
  QString getTestBinaryPath();
  void dumpDir(const QString& dir);
  QString getTestConfigPath(const QString& configName);
  bool waitForDiscovery(int timeoutMs = 5000);
  qint64 calculateLatency(const MessageRecord& sent, const MessageRecord& received);
}

// Forward declaration
class DiscoveryManager;

// Poll until at least 'minPeers' shows up or timeout
bool waitForPeers(DiscoveryManager& disc, const QString& topic, int minPeers,
                  int timeoutMs = 2000, int stepMs = 50);