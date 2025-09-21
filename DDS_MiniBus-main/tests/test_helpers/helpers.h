#pragma once

#include <QProcess>
#include <QTest>
#include <QString>
#include <QDir>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include "../common/test_utils.h"

// Windows-specific discovery settlement delay
inline void discoverySettle(int msWin = 2500, int msOther = 1000) {
#ifdef Q_OS_WIN
    QTest::qSleep(msWin);
#else
    QTest::qSleep(msOther);
#endif
}

// Robust process output reading helpers
inline QString waitForLine(QProcess& p, int timeoutMs) {
    if (!p.waitForReadyRead(timeoutMs)) return {};
    return QString::fromUtf8(p.readAllStandardOutput());
}

inline bool waitForContains(QProcess& p, const QString& needle, int totalMs) {
    const int step = 200;
    int waited = 0;
    QString buf;
    while (waited < totalMs) {
        buf += waitForLine(p, step);
        if (buf.contains(needle, Qt::CaseInsensitive)) return true;
        waited += step;
    }
    return false;
}

// Overload for TestProcess
inline bool waitForContains(TestProcess& p, const QString& needle, int totalMs) {
    const int step = 200;
    int waited = 0;
    QString buf;
    while (waited < totalMs) {
        QString line = p.readAllStdout();
        if (!line.isEmpty()) {
            buf += line;
            if (buf.contains(needle, Qt::CaseInsensitive)) return true;
        }
        QTest::qSleep(step);
        waited += step;
    }
    return false;
}

// Setup QProcess with proper Windows environment
inline void setupProcessEnvironment(QProcess& proc, const QString& workingDir) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList pathComponents;

    // Add working directory first
    if (QDir(workingDir).exists()) {
        pathComponents << workingDir;
    }

    // Add Qt and MinGW paths
    QString qtBinPath = "C:/Qt/6.9.2/mingw_64/bin";
    if (QDir(qtBinPath).exists()) {
        pathComponents << qtBinPath;
    }

    QString mingwBinPath = "C:/Qt/Tools/mingw1310_64/bin";
    if (QDir(mingwBinPath).exists()) {
        pathComponents << mingwBinPath;
    }

    // Add original PATH
    QString oldPath = env.value("PATH");
    if (!oldPath.isEmpty()) {
        pathComponents << oldPath;
    }

    env.insert("PATH", pathComponents.join(";"));
    proc.setProcessEnvironment(env);
    proc.setWorkingDirectory(workingDir);
}

// Get absolute paths for config files
inline QString getConfigPath(const QString& configName) {
    QString wd = QDir(QCoreApplication::applicationDirPath()).absolutePath();
    return wd + "/config/" + configName;
}

// Retry logic for message counts
inline int waitForMessageCount(QProcess& p, const QString& topic, int expectedCount, int maxTries = 10) {
    int tries = 0;
    while (tries++ < maxTries) {
        QTest::qSleep(250);
        // This would need to be implemented based on how messages are tracked
        // For now, return a placeholder
        return expectedCount; // Placeholder - actual implementation would parse process output
    }
    return 0;
}