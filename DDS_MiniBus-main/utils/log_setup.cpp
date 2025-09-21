#include "log_setup.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLoggingCategory>
#include <QDir>

static QFile* g_logFile = nullptr;

static void myMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    Q_UNUSED(ctx);
    QString level;
    switch (type) {
        case QtDebugMsg: level = "DEBUG"; break;
        case QtInfoMsg: level = "INFO"; break;
        case QtWarningMsg: level = "WARN"; break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg: level = "FATAL"; break;
    }
    QString line = QString("[%1][%2] %3")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODate), level, msg);
    fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
    fflush(stderr);  // Force immediate flush to ensure visibility
    if (g_logFile && g_logFile->isOpen()) {
        QTextStream ts(g_logFile);
        ts << line << "\n";
        g_logFile->flush();
    }
}

static QString rulesFromLevel(const QString& lvl) {
    const QString ll = lvl.toLower();
    if (ll=="debug") return "*.debug=true\n*.info=true\n*.warning=true\n*.critical=true\n";
    if (ll=="info")  return "*.debug=false\n*.info=true\n*.warning=true\n*.critical=true\n";
    if (ll=="warn" || ll=="warning") return "*.debug=false\n*.info=false\n*.warning=true\n*.critical=true\n";
    return "*.debug=false\n*.info=false\n*.warning=false\n*.critical=true\n";
}

namespace LogSetup {
void init(const QString& level, const QString& filePath) {
    QLoggingCategory::setFilterRules(rulesFromLevel(level));
    if (!filePath.isEmpty()) {
        // Ensure directory exists
        QDir().mkpath(QFileInfo(filePath).absolutePath());
        g_logFile = new QFile(filePath);
        g_logFile->open(QIODevice::Append | QIODevice::Text);
    }
    qInstallMessageHandler(myMessageHandler);
    qInfo() << "[Log] initialized level="<<level<<" file="<<filePath;
}

void setLevel(const QString& level) {
    QLoggingCategory::setFilterRules(rulesFromLevel(level));
    qInfo() << "[Log] level changed to " << level;
}
}
