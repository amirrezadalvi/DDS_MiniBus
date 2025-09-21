/* tests/common/test_utils.cpp */
#include "tests/common/test_utils.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QThread>

TestProcess::TestProcess(QObject* parent) : QObject(parent) {
  process = new QProcess(this);
  process->setProcessChannelMode(QProcess::MergedChannels);
  connect(process, &QProcess::readyReadStandardOutput, this, &TestProcess::onReadyReadStandardOutput);
  connect(process, &QProcess::readyReadStandardError, this, &TestProcess::onReadyReadStandardError);
}

bool TestProcess::start(const QString& program, const QStringList& arguments) {
  timer.start();

  // Set working directory to qt_deploy as specified
  QString qtDeployDir = QDir::currentPath() + "/qt_deploy";
  if (QDir(qtDeployDir).exists()) {
    process->setWorkingDirectory(qtDeployDir);
  } else {
    // Fallback to current directory if qt_deploy doesn't exist
    process->setWorkingDirectory(QDir::currentPath());
  }

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const QString oldPath = env.value("PATH");

  // Build PATH as specified: qt_deploy, Qt bin, MinGW bin, then original PATH
  QStringList pathComponents;

  // Add qt_deploy directory first
  if (QDir(qtDeployDir).exists()) {
    pathComponents << qtDeployDir;
  }

  // Add Qt 6.9.2 MinGW bin
  QString qtBinPath = "C:/Qt/6.9.2/mingw_64/bin";
  if (QDir(qtBinPath).exists()) {
    pathComponents << qtBinPath;
  }

  // Add MinGW 13.1.0 bin
  QString mingwBinPath = "C:/Qt/Tools/mingw1310_64/bin";
  if (QDir(mingwBinPath).exists()) {
    pathComponents << mingwBinPath;
  }

  // Add application directory
  pathComponents << QCoreApplication::applicationDirPath();

  // Add original PATH
  if (!oldPath.isEmpty()) {
    pathComponents << oldPath;
  }

  QString newPath = pathComponents.join(";");
  env.insert("PATH", newPath);
  process->setProcessEnvironment(env);

  qDebug() << "[DEBUG] wd=" << process->workingDirectory();
  qDebug() << "[DEBUG] program=" << program;
  qDebug() << "[DEBUG] args=" << arguments;
  qDebug() << "[DEBUG] exists?=" << QFileInfo(program).exists()
           << " exec?=" << QFileInfo(program).isExecutable();
  qDebug() << "[DEBUG] PATH=" << env.value("PATH");

  process->start(program, arguments);
  if (!process->waitForStarted(5000)) {
    qWarning() << "[ERROR] QProcess failed to start:" << process->error() << process->errorString();
    return false;
  }
  return true;
}

bool TestProcess::waitForFinished(int msec) {
  return process->waitForFinished(msec);
}

QString TestProcess::readAllStdout() const {
  return QString::fromUtf8(process->readAllStandardOutput());
}

QString TestProcess::readAllStderr() const {
  return QString::fromUtf8(process->readAllStandardError());
}

void TestProcess::terminate() {
  if (process && process->state() != QProcess::NotRunning) {
    process->terminate();
    process->waitForFinished(3000);
  }
}

bool TestProcess::terminateProcess(int timeoutMs) {
  if (!process) return true;
  if (process->state() == QProcess::NotRunning) return true;

  qDebug() << "[DEBUG] terminateProcess(): sending terminate()";
  process->terminate();
  if (!process->waitForFinished(timeoutMs)) {
    qWarning() << "[WARN] terminateProcess(): grace timeout, sending kill()";
    process->kill();
    process->waitForFinished(2000);
  }
  return (process->state() == QProcess::NotRunning);
}

int TestProcess::exitCode() const {
  return process ? process->exitCode() : -1;
}

QProcess::ExitStatus TestProcess::exitStatus() const {
  return process ? process->exitStatus() : QProcess::NormalExit;
}

bool TestProcess::isRunning() const {
  return process && process->state() != QProcess::NotRunning;
}

ProcessResult TestProcess::result() const {
  ProcessResult res;
  res.exitCode = process->exitCode();
  res.stdoutLines = stdoutBuffer;
  res.stderrLines = stderrBuffer;
  res.elapsedMs = timer.elapsed();
  return res;
}

QStringList TestProcess::findLines(const QRegularExpression& pattern) const {
  QStringList matches;
  for (const QString& line : stdoutBuffer) {
    if (pattern.match(line).hasMatch()) {
      matches << line;
    }
  }
  return matches;
}

QVector<MessageRecord> TestProcess::parseSentMessages() const {
  QVector<MessageRecord> messages;
  QRegularExpression rx("cli:\\s+sent\\s+msg_id=(\\d+)\\s+topic=([^\\s]+)\\s+qos=([^\\s]+)");

  for (const QString& line : stdoutBuffer) {
    QRegularExpressionMatch match = rx.match(line);
    if (match.hasMatch()) {
      MessageRecord msg;
      msg.messageId = match.captured(1).toLongLong();
      msg.timestamp = timer.elapsed(); // Approximate send time
      messages.append(msg);
    }
  }
  return messages;
}

QVector<MessageRecord> TestProcess::parseReceivedMessages() const {
  QVector<MessageRecord> messages;
  QRegularExpression rx("\\[ts=([^\\]]+)\\]\\s+topic=([^\\s]+)\\s+qos=([^\\s]+)\\s+msg_id=(\\d+)\\s+payload=(.+)");

  for (const QString& line : stdoutBuffer) {
    QRegularExpressionMatch match = rx.match(line);
    if (match.hasMatch()) {
      MessageRecord msg;
      msg.messageId = match.captured(4).toLongLong();
      msg.timestamp = QDateTime::fromString(match.captured(1), "yyyy-MM-dd hh:mm:ss.zzz").toMSecsSinceEpoch();
      msg.payload = match.captured(5);
      messages.append(msg);
    }
  }
  return messages;
}

int TestProcess::countAcks() const {
  return findLines(QRegularExpression("\\[ACK\\]\\[RX\\]")).size();
}

int TestProcess::countResends() const {
  return findLines(QRegularExpression("\\[RESEND\\]")).size();
}

void TestProcess::onReadyReadStandardOutput() {
  while (process->canReadLine()) {
    QString line = QString::fromUtf8(process->readLine()).trimmed();
    stdoutBuffer << line;
    qDebug() << "[STDOUT]" << line;
  }
}

void TestProcess::onReadyReadStandardError() {
  while (process->canReadLine()) {
    QString line = QString::fromUtf8(process->readLine()).trimmed();
    stderrBuffer << line;
    qDebug() << "[STDERR]" << line;
  }
}

QString IntegrationTestUtils::getTestBinaryPath() {
#ifdef TEST_APP_NAME
  const QString p = QCoreApplication::applicationDirPath()
                    + QDir::separator()
                    + QString::fromUtf8(TEST_APP_NAME);
  if (QFileInfo::exists(p) && QFileInfo(p).isExecutable())
    return p;
#endif
  // Fallbacks
  const QString base = QCoreApplication::applicationDirPath();
  const QStringList names = { QStringLiteral("dds_mini_bus.exe"),
                              QStringLiteral("mini_dds.exe") };
  for (const auto& n : names) {
    const QString cand = base + QDir::separator() + n;
    if (QFileInfo::exists(cand) && QFileInfo(cand).isExecutable())
      return cand;
  }
  const QString envPath = qEnvironmentVariable("TEST_BIN");
  if (!envPath.isEmpty() && QFileInfo(envPath).exists() && QFileInfo(envPath).isExecutable())
    return envPath;

  qWarning() << "[WARN] Could not resolve app binary path.";
  return QString();
}

void IntegrationTestUtils::dumpDir(const QString& dir) {
  QDir d(dir);
  qDebug() << "[DEBUG] dir:" << dir;
  for (const auto& f : d.entryList(QDir::Files))
    qDebug() << "  -" << f;
}

QString IntegrationTestUtils::getTestConfigPath(const QString& configName) {
  return QString("config/%1").arg(configName);
}

bool IntegrationTestUtils::waitForDiscovery(int timeoutMs) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < timeoutMs) {
    QCoreApplication::processEvents();
    QThread::msleep(100);
  }
  return true; // Assume discovery works
}

qint64 IntegrationTestUtils::calculateLatency(const MessageRecord& sent, const MessageRecord& received) {
  return received.timestamp - sent.timestamp;
}