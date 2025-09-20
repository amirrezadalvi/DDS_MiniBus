#include "logger.h"
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(LogDisc, "dds.discovery")
Q_LOGGING_CATEGORY(LogQoS,  "dds.qos")
Q_LOGGING_CATEGORY(LogNet,  "dds.net")
Q_LOGGING_CATEGORY(LogCore, "dds.core")

void Logger::setLevel(const QString& level) {
    QtMsgType msgType = QtInfoMsg;
    if (level == "debug") msgType = QtDebugMsg;
    else if (level == "info") msgType = QtInfoMsg;
    else if (level == "warn") msgType = QtWarningMsg;
    else if (level == "error") msgType = QtCriticalMsg;
    // Set the global message type threshold
    QLoggingCategory::setFilterRules(QString("*.%1=true").arg(level));
}
