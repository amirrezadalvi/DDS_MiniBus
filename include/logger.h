#pragma once
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(LogDisc)
Q_DECLARE_LOGGING_CATEGORY(LogQoS)
Q_DECLARE_LOGGING_CATEGORY(LogNet)
Q_DECLARE_LOGGING_CATEGORY(LogCore)

class Logger {
public:
    static void setLevel(const QString& level);
};
