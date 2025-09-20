#include "config_manager.h"
#include "logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>

ConfigManager& ConfigManager::ref() {
    static ConfigManager i;
    return i;
}

bool ConfigManager::load(const QString& path, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("open failed: %1").arg(path);
        return false;
    }
    const auto bytes = f.readAll();
    const auto doc   = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) {
        if (err) *err = QStringLiteral("root not object");
        return false;
    }
    configPath = path;
    parse(doc.object());
    return true;
}

// helper: parse "host:port" -> {host,port}
static inline bool parseHostPort(const QString& s, QString& hostOut, quint16& portOut) {
    const auto parts = s.split(':');
    if (parts.size() != 2) return false;
    bool ok = false;
    const auto p = parts[1].toUShort(&ok);
    if (!ok) return false;
    hostOut = parts[0];
    portOut = p;
    return true;
}

void ConfigManager::parse(const QJsonObject& o) {
    // ---- root keys ----
    node_id = o.value(QStringLiteral("node_id")).toString(node_id);
    protocol_version = o.value(QStringLiteral("protocol_version")).toString(protocol_version);

    // ---- discovery ----
    if (o.contains(QStringLiteral("discovery"))) {
        const auto d = o.value(QStringLiteral("discovery")).toObject();
        disc.enabled     = d.value(QStringLiteral("enabled")).toBool(disc.enabled);
        disc.mode        = d.value(QStringLiteral("mode")).toString(disc.mode);
        disc.address     = QHostAddress(d.value(QStringLiteral("address")).toString(disc.address.toString()));
        disc.port        = static_cast<quint16>(d.value(QStringLiteral("port")).toInt(disc.port));
        disc.interval_ms = d.value(QStringLiteral("interval_ms")).toInt(disc.interval_ms);
        disc.ttl = d.value(QStringLiteral("ttl")).toInt(disc.ttl);

        // Validation
        if (disc.mode != "broadcast" && disc.mode != "multicast") {
            qWarning() << "[Config] discovery.mode must be 'broadcast' or 'multicast', got:" << disc.mode;
            disc.mode = "broadcast";
        }
        if (disc.port < 1024 || disc.port > 65535) {
            qWarning() << "[Config] discovery.port out of range (1024-65535), got:" << disc.port;
            disc.port = 39001;
        }
        if (disc.interval_ms < 200) {
            qWarning() << "[Config] discovery.interval_ms too low, got:" << disc.interval_ms;
            disc.interval_ms = 200;
        }
        if (disc.mode == "multicast") {
            QHostAddress addr = disc.address;
            quint32 ip = addr.toIPv4Address();
            if (!(ip >= 0xE0000000 && ip <= 0xEFFFFFFF)) { // 224.0.0.0/4
                qWarning() << "[Config] discovery.address is not a valid IPv4 multicast address:" << addr.toString();
                disc.address = QHostAddress("239.255.0.1");
            }
        }
    }

    // ---- transport ----
    if (o.contains(QStringLiteral("transport"))) {
        const auto t = o.value(QStringLiteral("transport")).toObject();
        transport.default_protocol = t.value(QStringLiteral("default")).toString(transport.default_protocol);

        // UDP
        if (t.contains(QStringLiteral("udp"))) {
            const auto u = t.value(QStringLiteral("udp")).toObject();
            transport.udp.port   = static_cast<quint16>(u.value(QStringLiteral("port")).toInt(transport.udp.port));
            transport.udp.rcvbuf = u.value(QStringLiteral("rcvbuf")).toInt(transport.udp.rcvbuf);
            transport.udp.sndbuf = u.value(QStringLiteral("sndbuf")).toInt(transport.udp.sndbuf);
        }

        // TCP
        if (t.contains(QStringLiteral("tcp"))) {
            const auto jtcp = t.value(QStringLiteral("tcp")).toObject();
            transport.tcp.listen  = jtcp.value(QStringLiteral("listen")).toBool(transport.tcp.listen);
            transport.tcp.port    = static_cast<quint16>(jtcp.value(QStringLiteral("port")).toInt(transport.tcp.port));
            transport.tcp.rcvbuf  = jtcp.value(QStringLiteral("rcvbuf")).toInt(transport.tcp.rcvbuf);
            transport.tcp.sndbuf  = jtcp.value(QStringLiteral("sndbuf")).toInt(transport.tcp.sndbuf);
            transport.tcp.connect_timeout_ms   = jtcp.value(QStringLiteral("connect_timeout_ms")).toInt(transport.tcp.connect_timeout_ms);
            transport.tcp.heartbeat_ms         = jtcp.value(QStringLiteral("heartbeat_ms")).toInt(transport.tcp.heartbeat_ms);
            transport.tcp.reconnect_backoff_ms = jtcp.value(QStringLiteral("reconnect_backoff_ms")).toInt(transport.tcp.reconnect_backoff_ms);
            transport.tcp.max_reconnect_attempts = jtcp.value(QStringLiteral("max_reconnect_attempts")).toInt(transport.tcp.max_reconnect_attempts);

            transport.tcp.connect.clear();
            if (jtcp.contains(QStringLiteral("connect"))) {
                const auto arr = jtcp.value(QStringLiteral("connect")).toArray();
                for (const auto& v : arr) {
                    if (v.isString()) {
                        QString h; quint16 p=0;
                        if (parseHostPort(v.toString(), h, p)) {
                            transport.tcp.connect.push_back({h, p});
                        }
                    } else if (v.isObject()) {
                        const auto obj = v.toObject();
                        const QString h = obj.value(QStringLiteral("host")).toString();
                        const int pInt  = obj.value(QStringLiteral("port")).toInt(-1);
                        if (!h.isEmpty() && pInt >= 0 && pInt <= 65535) {
                            transport.tcp.connect.push_back({h, static_cast<quint16>(pInt)});
                        }
                    }
                }
            }
        }
    }

    // ---- qos ----
    if (o.contains(QStringLiteral("qos"))) {
        const auto q = o.value(QStringLiteral("qos")).toObject();
        qos_cfg.def = q.value(QStringLiteral("default")).toString(qos_cfg.def);
        if (q.contains(QStringLiteral("reliable"))) {
            const auto r = q.value(QStringLiteral("reliable")).toObject();
            qos_cfg.reliable.ack_timeout_ms = r.value(QStringLiteral("ack_timeout_ms")).toInt(qos_cfg.reliable.ack_timeout_ms);
            qos_cfg.reliable.max_retries    = r.value(QStringLiteral("max_retries")).toInt(qos_cfg.reliable.max_retries);
            qos_cfg.reliable.exponential_backoff = r.value(QStringLiteral("exponential_backoff"))
                                                    .toBool(qos_cfg.reliable.exponential_backoff);
        }
        qos_cfg.dedup_capacity = q.value(QStringLiteral("dedup_capacity")).toInt(qos_cfg.dedup_capacity);
        if (q.contains(QStringLiteral("retain_last"))) {
          qos_cfg.retain_last = q.value(QStringLiteral("retain_last")).toBool(qos_cfg.retain_last);
        }
    }

    // ---- topics ----
    topics_list.clear();
    if (o.contains(QStringLiteral("topics"))) {
        const auto ta = o.value(QStringLiteral("topics")).toArray();
        for (const auto& v : ta) topics_list << v.toString();
    }

    // ---- serialization ----
    if (o.contains(QStringLiteral("serialization"))) {
        const auto s = o.value(QStringLiteral("serialization")).toObject();
        serialization.format = s.value(QStringLiteral("format")).toString(serialization.format);

        // Parse supported formats, default to [format, other]
        if (s.contains(QStringLiteral("supported"))) {
            const auto arr = s.value(QStringLiteral("supported")).toArray();
            serialization.supported.clear();
            for (const auto& v : arr) {
                if (v.isString()) serialization.supported << v.toString();
            }
        } else {
            // Default: put our format first, then the other
            serialization.supported.clear();
            serialization.supported << serialization.format;
            if (serialization.format == "json") serialization.supported << "cbor";
            else serialization.supported << "json";
        }
        if (s.contains(QStringLiteral("allow_json_fallback"))) {
            serialization.allow_json_fallback = s.value(QStringLiteral("allow_json_fallback")).toBool(serialization.allow_json_fallback);
        }
    }

    // ---- logging ----
    if (o.contains(QStringLiteral("logging"))) {
        const auto l = o.value(QStringLiteral("logging")).toObject();
        logging.level = l.value(QStringLiteral("level")).toString(logging.level);
        logging.file  = l.value(QStringLiteral("file")).toString(logging.file);
        logging.deadletter_file = l.value(QStringLiteral("deadletter_file")).toString(logging.deadletter_file);
    }
}

void ConfigManager::startWatching(const QString& path) {
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, &ConfigManager::onConfigFileChanged);
    watcher.addPath(path);
}

void ConfigManager::onConfigFileChanged() {
    QString err;
    if (!load(configPath, &err)) {
        qWarning() << "[Config] Reload failed:" << err;
        return;
    }

    // Check for changes in reloadable fields
    // For now, assume we reload and emit if changed, but since parse overwrites, we need to track old values.

    // Actually, since parse overwrites the members, we need to store old values before reloading.

    // Better: reload into a temp config, compare, then update and emit.

    // For simplicity, since it's small, reload and emit always, but only for the fields.

    // But to detect changes, need old values.

    // Let's store old values at the beginning of onConfigFileChanged.

    QString oldLogLevel = logging.level;
    int oldDiscInterval = disc.interval_ms;

    if (!load(configPath, &err)) {
        qWarning() << "[Config] Reload failed:" << err;
        return;
    }

    // Log warnings for non-reloadable fields - but since we reloaded everything, we can't easily know what changed.
    // For now, just emit for the reloadable ones if they changed.

    if (logging.level != oldLogLevel) {
        emit configChangedLogLevel(logging.level);
    }

    if (disc.interval_ms != oldDiscInterval) {
        emit configChangedDiscoveryInterval(disc.interval_ms);
    }

    // Log warnings for other changes - but hard to detect without parsing twice.
    // For simplicity, assume only log_level and disc.interval_ms are changed, and warn for others if needed.
    // Actually, since the task says "Log warning for non-reloadable fields", but to do that, need to know what changed.
    // Perhaps parse the new config, compare each field, and if changed and not reloadable, warn.

    // To keep it simple, since only two are reloadable, and others are not, we can warn if any other field changed, but that's not precise.

    // The task: "Log warning for non-reloadable fields" - probably when reloading, if any non-reloadable field is different, warn that it's not reloaded.

    // So, need to parse the new config without applying, compare.

    // Let's modify: in onConfigFileChanged, load the file, parse into temp, compare reloadable, emit, and warn for others.

    // But since parse modifies the object, need a way to parse without modifying.

    // Perhaps have a parse method that takes a reference to update.

    // For now, implement a simple version: reload, emit for the two, and always warn that other fields are not reloadable (but that's not accurate).

    // Better: store all old values, reload, then check which changed, emit for reloadable, warn for others.

    // Yes.

    // So, let's do that.

    // At the top:

    QString oldNodeId = node_id;
    QString oldProtoVersion = protocol_version;
    DiscoveryConfig oldDisc = disc;
    TransportConfig oldTransport = transport;
    QosConfig oldQos = qos_cfg;
    SerializationConfig oldSerialization = serialization;
    QStringList oldTopics = topics_list;
    QString oldLogFile = logging.file;

    // Then after load:

    if (node_id != oldNodeId) qWarning() << "[Config] node_id changed but not reloadable";
    if (protocol_version != oldProtoVersion) qWarning() << "[Config] protocol_version changed but not reloadable";
    if (disc.enabled != oldDisc.enabled) qWarning() << "[Config] discovery.enabled changed but not reloadable";
    if (disc.mode != oldDisc.mode) qWarning() << "[Config] discovery.mode changed but not reloadable";
    if (disc.address != oldDisc.address) qWarning() << "[Config] discovery.address changed but not reloadable";
    if (disc.port != oldDisc.port) qWarning() << "[Config] discovery.port changed but not reloadable";
    // interval_ms is reloadable
    if (transport.default_protocol != oldTransport.default_protocol) qWarning() << "[Config] transport.default changed but not reloadable";
    if (transport.udp.port != oldTransport.udp.port) qWarning() << "[Config] transport.udp.port changed but not reloadable";
    if (transport.udp.rcvbuf != oldTransport.udp.rcvbuf) qWarning() << "[Config] transport.udp.rcvbuf changed but not reloadable";
    if (transport.udp.sndbuf != oldTransport.udp.sndbuf) qWarning() << "[Config] transport.udp.sndbuf changed but not reloadable";
    if (transport.tcp.listen != oldTransport.tcp.listen) qWarning() << "[Config] transport.tcp.listen changed but not reloadable";
    if (transport.tcp.port != oldTransport.tcp.port) qWarning() << "[Config] transport.tcp.port changed but not reloadable";
    if (transport.tcp.rcvbuf != oldTransport.tcp.rcvbuf) qWarning() << "[Config] transport.tcp.rcvbuf changed but not reloadable";
    if (transport.tcp.sndbuf != oldTransport.tcp.sndbuf) qWarning() << "[Config] transport.tcp.sndbuf changed but not reloadable";
    if (transport.tcp.connect_timeout_ms != oldTransport.tcp.connect_timeout_ms) qWarning() << "[Config] transport.tcp.connect_timeout_ms changed but not reloadable";
    if (transport.tcp.heartbeat_ms != oldTransport.tcp.heartbeat_ms) qWarning() << "[Config] transport.tcp.heartbeat_ms changed but not reloadable";
    if (transport.tcp.reconnect_backoff_ms != oldTransport.tcp.reconnect_backoff_ms) qWarning() << "[Config] transport.tcp.reconnect_backoff_ms changed but not reloadable";
    if (transport.tcp.max_reconnect_attempts != oldTransport.tcp.max_reconnect_attempts) qWarning() << "[Config] transport.tcp.max_reconnect_attempts changed but not reloadable";
    if (qos_cfg.def != oldQos.def) qWarning() << "[Config] qos.default changed but not reloadable";
    if (qos_cfg.reliable.ack_timeout_ms != oldQos.reliable.ack_timeout_ms) qWarning() << "[Config] qos.reliable.ack_timeout_ms changed but not reloadable";
    if (qos_cfg.reliable.max_retries != oldQos.reliable.max_retries) qWarning() << "[Config] qos.reliable.max_retries changed but not reloadable";
    if (qos_cfg.reliable.exponential_backoff != oldQos.reliable.exponential_backoff) qWarning() << "[Config] qos.reliable.exponential_backoff changed but not reloadable";
    if (qos_cfg.dedup_capacity != oldQos.dedup_capacity) qWarning() << "[Config] qos.dedup_capacity changed but not reloadable";
    if (serialization.format != oldSerialization.format) qWarning() << "[Config] serialization.format changed but not reloadable";
    if (topics_list != oldTopics) qWarning() << "[Config] topics changed but not reloadable";
    if (logging.file != oldLogFile) qWarning() << "[Config] logging.file changed but not reloadable";

    // Emit for reloadable
    if (logging.level != oldLogLevel) {
        emit configChangedLogLevel(logging.level);
    }
    if (disc.interval_ms != oldDiscInterval) {
        emit configChangedDiscoveryInterval(disc.interval_ms);
    }

    qInfo() << "[Config] reloaded";
}

void ConfigManager::applyReloadableDiff(const ConfigManager& oldCfg) {
    // Reloadable: logging.level
    if (logging.level != oldCfg.logging.level) {
        Logger::setLevel(logging.level);
        emit configChangedLogLevel(logging.level);
    }
    // qos.reliable.* and retain_last are read at runtime, so no need to re-init
    if (qos_cfg.reliable.ack_timeout_ms != oldCfg.qos_cfg.reliable.ack_timeout_ms ||
        qos_cfg.reliable.max_retries != oldCfg.qos_cfg.reliable.max_retries ||
        qos_cfg.reliable.exponential_backoff != oldCfg.qos_cfg.reliable.exponential_backoff ||
        qos_cfg.retain_last != oldCfg.qos_cfg.retain_last) {
        qCInfo(LogCore) << "[Config] QoS settings reloaded";
    }
    emit reloaded();
}
