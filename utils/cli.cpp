#include "cli.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QStringList>

std::optional<CliOptions> CliParser::parse(int argc, char* argv[]) {
    CliOptions opts;
    QStringList args;
    for (int i = 1; i < argc; ++i) {
        args << QString::fromUtf8(argv[i]);
    }

    for (int i = 0; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (arg == "--help" || arg == "-h") {
            opts.help = true;
            return opts;
        } else if (arg == "--role") {
            opts.role = getArgValue(args, "--role", i);
            if (opts.role != "sender" && opts.role != "subscriber") {
                qCritical() << "Invalid role:" << opts.role << "(must be 'sender' or 'subscriber')";
                return std::nullopt;
            }
        } else if (arg == "--topic") {
            opts.topic = getArgValue(args, "--topic", i);
        } else if (arg == "--qos") {
            opts.qos = getArgValue(args, "--qos", i);
            if (opts.qos != "reliable" && opts.qos != "best_effort") {
                qCritical() << "Invalid qos:" << opts.qos << "(must be 'reliable' or 'best_effort')";
                return std::nullopt;
            }
        } else if (arg == "--count") {
            bool ok;
            opts.count = getArgValue(args, "--count", i).toInt(&ok);
            if (!ok || opts.count < 1) {
                qCritical() << "Invalid count:" << opts.count << "(must be >= 1)";
                return std::nullopt;
            }
        } else if (arg == "--interval-ms") {
            bool ok;
            opts.interval_ms = getArgValue(args, "--interval-ms", i).toInt(&ok);
            if (!ok || opts.interval_ms < 10) {
                qCritical() << "Invalid interval-ms:" << opts.interval_ms << "(must be >= 10)";
                return std::nullopt;
            }
        } else if (arg == "--payload") {
            QString jsonStr = getArgValue(args, "--payload", i);
            opts.payload = parseJsonPayload(jsonStr);
            if (opts.payload.isEmpty()) {
                qCritical() << "Invalid JSON payload:" << jsonStr;
                return std::nullopt;
            }
        } else if (arg == "--config") {
            opts.config_path = getArgValue(args, "--config", i);
        } else if (arg == "--log-level") {
            opts.log_level = getArgValue(args, "--log-level", i);
            if (opts.log_level != "debug" && opts.log_level != "info" &&
                opts.log_level != "warn" && opts.log_level != "error") {
                qCritical() << "Invalid log-level:" << opts.log_level << "(must be 'debug', 'info', 'warn', or 'error')";
                return std::nullopt;
            }
        } else if (arg == "--start-delay-ms") {
            bool ok;
            opts.startDelayMs = getArgValue(args, "--start-delay-ms", i).toInt(&ok);
            if (!ok || opts.startDelayMs < 0) {
                qCritical() << "Invalid start-delay-ms:" << opts.startDelayMs << "(must be >= 0)";
                return std::nullopt;
            }
        } else if (arg == "--print-recv") {
            opts.printRecv = true;
        } else if (arg == "--run-for-sec") {
            bool ok;
            opts.runForSec = getArgValue(args, "--run-for-sec", i).toInt(&ok);
            if (!ok || opts.runForSec < 1) {
                qCritical() << "Invalid run-for-sec:" << opts.runForSec << "(must be >= 1)";
                return std::nullopt;
            }
        } else {
            qCritical() << "Unknown argument:" << arg;
            return std::nullopt;
        }
    }

    if (opts.role.isEmpty()) {
        qCritical() << "Missing required --role argument";
        return std::nullopt;
    }

    return opts;
}

void CliParser::printHelp() {
    qInfo() << "DDS Mini-Bus CLI";
    qInfo() << "";
    qInfo() << "Usage: mini_dds [options]";
    qInfo() << "";
    qInfo() << "Options:";
    qInfo() << "  --role <sender|subscriber>    Required: Run as sender or subscriber";
    qInfo() << "  --topic <string>              Topic to publish/subscribe (default: sensor/temperature)";
    qInfo() << "  --qos <reliable|best_effort>  QoS level (default: reliable)";
    qInfo() << "  --count <int>                 Number of messages to send (default: 1, sender only)";
    qInfo() << "  --interval-ms <int>           Interval between messages in ms (default: 1000, sender only)";
    qInfo() << "  --payload <json>              JSON payload for messages (default: {\"value\":23.5,\"unit\":\"C\"}, sender only)";
    qInfo() << "  --config <path>               Config file path (default: config/config.json)";
    qInfo() << "  --log-level <debug|info|warn|error>  Log level (default: info)";
    qInfo() << "  --start-delay-ms <int>        Delay before starting sender in ms (default: 0, sender only)";
    qInfo() << "  --print-recv                  Print received messages to stdout (subscriber only)";
    qInfo() << "  --run-for-sec <int>           Run for N seconds then exit cleanly (default: 0 = run indefinitely)";
    qInfo() << "  --help, -h                    Show this help message";
    qInfo() << "";
    qInfo() << "Examples:";
    qInfo() << "  ./mini_dds --role sender --topic sensor/temp --qos reliable --count 5";
    qInfo() << "  ./mini_dds --role subscriber --topic sensor/temp --log-level debug";
}

QString CliParser::getArgValue(const QStringList& args, const QString& flag, int& i) {
    if (i + 1 >= args.size()) {
        qCritical() << "Missing value for" << flag;
        return QString();
    }
    return args[++i];
}

QJsonObject CliParser::parseJsonPayload(const QString& jsonStr) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}