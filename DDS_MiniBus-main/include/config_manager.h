#pragma once
#include <QObject>
#include <QHostAddress>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>
#include <QJsonObject>
#include <QFileSystemWatcher>

struct DiscoveryConfig {
    bool enabled = true;
    QString mode = "broadcast";                // "broadcast" | "multicast"
    QHostAddress address = QHostAddress(QStringLiteral("239.255.0.1"));
    quint16 port = 45454;
    int interval_ms = 1000;
    int ttl = 1; // optional, for multicast hops
};

struct UdpConfig {
    quint16 port = 38020;
    int rcvbuf = 262144;                       // bytes
    int sndbuf = 262144;                       // bytes
};

struct TcpConnectTarget {
    QString host;
    quint16 port = 0;
};

struct TcpConfig {
    bool listen = true;
    quint16 port = 38030;
    QList<TcpConnectTarget> connect;           // outgoing targets
    int rcvbuf = 262144;
    int sndbuf = 262144;
    int connect_timeout_ms = 1500;
    int heartbeat_ms = 0;                      // 0 = disabled
    int reconnect_backoff_ms = 500;
    int max_reconnect_attempts = 10;           // cap reconnect attempts
};

struct TransportConfig {
    QString default_protocol = "udp";          // "udp" | "tcp"
    UdpConfig udp;
    TcpConfig tcp;
};

struct QosReliable {
    int  ack_timeout_ms = 200;
    int  max_retries = 3;
    bool exponential_backoff = true;
};

struct QosConfig {
    QString def = "best_effort";               // "best_effort" | "reliable"
    QosReliable reliable;
    int dedup_capacity = 2048;                 // LRU capacity for de-duplication
    bool retain_last = false;                  // retain last message per topic
};

struct SerializationConfig {
    QString format = "json";                   // "json" | "cbor"
    QStringList supported = {"json", "cbor"};  // ordered by preference
    bool allow_json_fallback = true;
};

struct LoggingConfig {
    QString level = "info";
    QString file = "logs/dds.log";
    QString deadletter_file = "logs/dds_deadletter.ndjson";
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    int getInt(const QString& dotted, int def) const;
    std::string getString(const QString& dotted, const std::string& def) const;
    static ConfigManager& ref();

    bool load(const QString& path, QString* err = nullptr);
    void parse(const QJsonObject& o);
    void startWatching(const QString& path);

    // ---- Exposed settings (read-only via references) ----
    QString node_id = "node-1";
    QString protocol_version = "1.0";

    DiscoveryConfig disc;
    TransportConfig transport;
    QosConfig       qos_cfg;
    SerializationConfig serialization;
    LoggingConfig   logging;

    QStringList topics_list;

signals:
    void configChangedLogLevel(const QString& newLevel);
    void configChangedDiscoveryInterval(int newIntervalMs);
    void reloaded();

private slots:
    void onConfigFileChanged();

private:
    void applyReloadableDiff(const ConfigManager& oldCfg);
    QFileSystemWatcher watcher;
    QString configPath;
};
