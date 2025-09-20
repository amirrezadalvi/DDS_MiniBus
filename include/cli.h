#pragma once
#include <QString>
#include <QJsonObject>
#include <optional>

struct CliOptions {
    QString role; // "sender" or "subscriber"
    QString topic = "sensor/temperature";
    QString qos = "reliable";
    int count = 1;
    int interval_ms = 1000;
    QJsonObject payload;
    QString config_path = "config/config.json";
    QString log_level = "info";
    bool help = false;
    int startDelayMs = 0;      // NEW: delay before starting sender
    bool printRecv = false;    // NEW: print received messages to stdout
    int runForSec = 0;         // NEW: run for N seconds then exit

    // Helper methods
    bool isSender() const { return role == "sender"; }
    bool isSubscriber() const { return role == "subscriber"; }
    bool isReliable() const { return qos == "reliable"; }
};

class CliParser {
public:
    static std::optional<CliOptions> parse(int argc, char* argv[]);
    static void printHelp();
private:
    static QString getArgValue(const QStringList& args, const QString& flag, int& i);
    static QJsonObject parseJsonPayload(const QString& jsonStr);
};