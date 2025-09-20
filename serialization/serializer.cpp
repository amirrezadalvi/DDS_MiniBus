#include "serializer.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QCborMap>
#include <QCborArray>
#include <QCborValue>

QByteArray Serializer::encodeDiscovery(const QString& nodeId, const QStringList& topics,
                                       const QString& proto, qint64 ts, quint16 data_port,
                                       const QStringList& serialization) {
    QJsonObject o{
        {"type", "discovery"},
        {"node_id", nodeId},
        {"protocol_version", proto},
        {"timestamp", ts},
        {"topics", QJsonArray::fromStringList(topics)},
        {"data_port", int(data_port)}
    };
    if (!serialization.isEmpty()) {
        o["serialization"] = QJsonArray::fromStringList(serialization);
    }
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray Serializer::encodeData(const MessageEnvelope& m) {
    QJsonObject o{
        {"type", "data"},
        {"topic", m.topic},
        {"message_id", m.message_id},
        {"timestamp", m.timestamp},
        {"payload", m.payload},
        {"publisher_id", m.publisher_id},
        {"qos", m.qos}
    };
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray Serializer::encodeAck(qint64 messageId, const QString& receiverId, const QString& status, qint64 ts) {
    QJsonObject o{
        {"type", "ack"},
        {"message_id", messageId},
        {"receiver_node_id", receiverId},
        {"status", status},
        {"timestamp", ts}
    };
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

std::optional<QJsonObject> Serializer::decode(const QByteArray& bytes, PacketType* outType) {
    // Try CBOR first
    QCborParserError cborErr;
    QCborValue cbor = QCborValue::fromCbor(bytes, &cborErr);
    if (cborErr.error == QCborError::NoError && cbor.isMap()) {
        QCborMap map = cbor.toMap();
        if (map.contains(QCborValue("type"))) {
            auto result = decodeCBOR(bytes, outType);
            if (result.has_value()) {
                return result;
            }
        }
    }

    // Fallback to JSON
    QJsonParseError err{};
    auto doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[DROP][DECODE] parse error: CBOR=" << cborErr.errorString() << ", JSON=" << err.errorString();
        if (outType) *outType = PacketType::Unknown;
        return std::nullopt;
    }
    auto o = doc.object();
    const auto t = o.value("type").toString();
    PacketType pt = PacketType::Unknown;
    if (t == "discovery") pt = PacketType::Discovery;
    else if (t == "data") pt = PacketType::Data;
    else if (t == "ack") pt = PacketType::Ack;
    if (outType) *outType = pt;

    // Validate required fields
    if (pt == PacketType::Data) {
        if (!o.contains("topic") || !o.contains("message_id") || !o.contains("payload") ||
            !o.contains("publisher_id") || !o.contains("qos")) {
            qWarning() << "[DROP][DECODE] missing required field in data packet";
            return std::nullopt;
        }
    } else if (pt == PacketType::Ack) {
        if (!o.contains("message_id")) {
            qWarning() << "[DROP][DECODE] missing message_id in ack packet";
            return std::nullopt;
        }
        // Normalize receiver identity to "receiver_node_id"
        QString receiverId;
        if (o.contains("receiver_node_id")) receiverId = o.value("receiver_node_id").toString();
        else if (o.contains("receiverId")) receiverId = o.value("receiverId").toString();
        else if (o.contains("receiver")) receiverId = o.value("receiver").toString();
        else if (o.contains("to")) receiverId = o.value("to").toString();
        if (!receiverId.isEmpty()) o["receiver_node_id"] = receiverId;
    } else if (pt == PacketType::Discovery) {
        if (!o.contains("node_id") || !o.contains("topics") || !o.contains("data_port")) {
            qWarning() << "[DROP][DECODE] missing required field in discovery packet";
            return std::nullopt;
        }
    }
    return o;
}

// Format negotiation
QString Serializer::negotiateFormat(const QStringList& ourPrefs, const QStringList& peerPrefs) {
    if (peerPrefs.isEmpty()) {
        return ourPrefs.isEmpty() ? "json" : ourPrefs.first();
    }

    // Find first common format in our preference order
    for (const QString& ourFmt : ourPrefs) {
        if (peerPrefs.contains(ourFmt)) {
            return ourFmt;
        }
    }

    // No common format, fallback to JSON
    return "json";
}

// CBOR encoding functions
QByteArray Serializer::encodeDiscoveryCBOR(const QString& nodeId, const QStringList& topics,
                                           const QString& proto, qint64 ts, quint16 data_port,
                                           const QStringList& serialization) {
    QCborMap map;
    map[QCborValue("type")] = QCborValue("discovery");
    map[QCborValue("node_id")] = QCborValue(nodeId);
    map[QCborValue("protocol_version")] = QCborValue(proto);
    map[QCborValue("timestamp")] = QCborValue(ts);
    QCborArray topicsArray;
    for (const QString& topic : topics) {
        topicsArray.append(QCborValue(topic));
    }
    map[QCborValue("topics")] = topicsArray;
    map[QCborValue("data_port")] = QCborValue(data_port);
    if (!serialization.isEmpty()) {
        QCborArray serArray;
        for (const QString& fmt : serialization) {
            serArray.append(QCborValue(fmt));
        }
        map[QCborValue("serialization")] = serArray;
    }
    return map.toCborValue().toCbor();
}

QByteArray Serializer::encodeDataCBOR(const MessageEnvelope& m) {
    QCborMap map;
    map[QCborValue("type")] = QCborValue("data");
    map[QCborValue("topic")] = QCborValue(m.topic);
    map[QCborValue("message_id")] = QCborValue(m.message_id);
    map[QCborValue("timestamp")] = QCborValue(m.timestamp);
    map[QCborValue("publisher_id")] = QCborValue(m.publisher_id);
    map[QCborValue("qos")] = QCborValue(m.qos);

    // Convert QJsonObject payload to QCborMap
    QCborMap payloadMap;
    for (auto it = m.payload.begin(); it != m.payload.end(); ++it) {
        payloadMap[QCborValue(it.key())] = QCborValue::fromJsonValue(it.value());
    }
    map[QCborValue("payload")] = payloadMap;

    return map.toCborValue().toCbor();
}

QByteArray Serializer::encodeAckCBOR(qint64 messageId, const QString& receiverId, const QString& status, qint64 ts) {
    QCborMap map;
    map[QCborValue("type")] = QCborValue("ack");
    map[QCborValue("message_id")] = QCborValue(messageId);
    map[QCborValue("receiver_node_id")] = QCborValue(receiverId);
    map[QCborValue("status")] = QCborValue(status);
    map[QCborValue("timestamp")] = QCborValue(ts);
    return map.toCborValue().toCbor();
}

std::optional<QJsonObject> Serializer::decodeCBOR(const QByteArray& bytes, PacketType* outType) {
    QCborParserError err;
    QCborValue cbor = QCborValue::fromCbor(bytes, &err);
    if (err.error != QCborError::NoError || !cbor.isMap()) {
        qWarning() << "[DROP][DECODE] CBOR parse error:" << err.errorString();
        if (outType) *outType = PacketType::Unknown;
        return std::nullopt;
    }

    QCborMap map = cbor.toMap();
    QJsonObject o;

    // Convert QCborMap to QJsonObject
    for (auto it = map.begin(); it != map.end(); ++it) {
        QString key = it.key().toString();
        QCborValue value = it.value();

        if (value.isString()) {
            o[key] = value.toString();
        } else if (value.isInteger()) {
            o[key] = value.toInteger();
        } else if (value.isDouble()) {
            o[key] = value.toDouble();
        } else if (value.isBool()) {
            o[key] = value.toBool();
        } else if (value.isArray()) {
            QCborArray arr = value.toArray();
            QJsonArray jsonArr;
            for (const QCborValue& v : arr) {
                if (v.isString()) {
                    jsonArr.append(v.toString());
                } else {
                    jsonArr.append(QJsonValue::fromVariant(v.toVariant()));
                }
            }
            o[key] = jsonArr;
        } else if (value.isMap()) {
            // Handle nested objects (like payload)
            QCborMap nestedMap = value.toMap();
            QJsonObject nestedObj;
            for (auto nit = nestedMap.begin(); nit != nestedMap.end(); ++nit) {
                QString nkey = nit.key().toString();
                QCborValue nvalue = nit.value();
                if (nvalue.isString()) {
                    nestedObj[nkey] = nvalue.toString();
                } else if (nvalue.isInteger()) {
                    nestedObj[nkey] = nvalue.toInteger();
                } else if (nvalue.isDouble()) {
                    nestedObj[nkey] = nvalue.toDouble();
                } else if (nvalue.isBool()) {
                    nestedObj[nkey] = nvalue.toBool();
                } else {
                    nestedObj[nkey] = QJsonValue::fromVariant(nvalue.toVariant());
                }
            }
            o[key] = nestedObj;
        } else {
            o[key] = QJsonValue::fromVariant(value.toVariant());
        }
    }

    const auto t = o.value("type").toString();
    PacketType pt = PacketType::Unknown;
    if (t == "discovery") pt = PacketType::Discovery;
    else if (t == "data") pt = PacketType::Data;
    else if (t == "ack") pt = PacketType::Ack;
    if (outType) *outType = pt;

    // Validate required fields (same as JSON version)
    if (pt == PacketType::Data) {
        if (!o.contains("topic") || !o.contains("message_id") || !o.contains("payload") ||
            !o.contains("publisher_id") || !o.contains("qos")) {
            qWarning() << "[DROP][DECODE] missing required field in data packet";
            return std::nullopt;
        }
    } else if (pt == PacketType::Ack) {
        if (!o.contains("message_id")) {
            qWarning() << "[DROP][DECODE] missing message_id in ack packet";
            return std::nullopt;
        }
        // Normalize receiver identity to "receiver_node_id"
        QString receiverId;
        if (o.contains("receiver_node_id")) receiverId = o.value("receiver_node_id").toString();
        else if (o.contains("receiverId")) receiverId = o.value("receiverId").toString();
        else if (o.contains("receiver")) receiverId = o.value("receiver").toString();
        else if (o.contains("to")) receiverId = o.value("to").toString();
        if (!receiverId.isEmpty()) o["receiver_node_id"] = receiverId;
    } else if (pt == PacketType::Discovery) {
        if (!o.contains("node_id") || !o.contains("topics") || !o.contains("data_port")) {
            qWarning() << "[DROP][DECODE] missing required field in discovery packet";
            return std::nullopt;
        }
    }
    return o;
}

// Polymorphic encode/decode
QByteArray Serializer::encodeEnvelope(const MessageEnvelope& m, const QString& fmt) {
    if (fmt == "cbor") {
        return encodeDataCBOR(m);
    } else {
        return encodeData(m);
    }
}

std::optional<MessageEnvelope> Serializer::decodeEnvelope(const QByteArray& bytes, const QString& fmt) {
    if (fmt == "cbor") {
        PacketType t = PacketType::Unknown;
        auto parsed = decodeCBOR(bytes, &t);
        if (!parsed || t != PacketType::Data) return std::nullopt;
        auto o = *parsed;
        MessageEnvelope m;
        m.topic = o.value("topic").toString();
        m.message_id = o.value("message_id").toVariant().toLongLong();
        m.payload = o.value("payload").toObject();
        m.timestamp = o.value("timestamp").toVariant().toLongLong();
        m.qos = o.value("qos").toString();
        m.publisher_id = o.value("publisher_id").toString();
        return m;
    } else {
        PacketType t = PacketType::Unknown;
        auto parsed = decode(bytes, &t);
        if (!parsed || t != PacketType::Data) return std::nullopt;
        auto o = *parsed;
        MessageEnvelope m;
        m.topic = o.value("topic").toString();
        m.message_id = o.value("message_id").toVariant().toLongLong();
        m.payload = o.value("payload").toObject();
        m.timestamp = o.value("timestamp").toVariant().toLongLong();
        m.qos = o.value("qos").toString();
        m.publisher_id = o.value("publisher_id").toString();
        return m;
    }
}

QByteArray Serializer::encodeDiscovery(const DiscoveryPacket& pkt, const QString& fmt) {
    if (fmt == "cbor") {
        return encodeDiscoveryCBOR(pkt.node_id, pkt.topics, pkt.protocol_version, pkt.timestamp, pkt.data_port, pkt.serialization);
    } else {
        return QJsonDocument(to_json(pkt)).toJson(QJsonDocument::Compact);
    }
}

std::optional<DiscoveryPacket> Serializer::decodeDiscovery(const QByteArray& bytes, const QString& fmt) {
    if (fmt == "cbor") {
        PacketType t = PacketType::Unknown;
        auto parsed = decodeCBOR(bytes, &t);
        if (!parsed || t != PacketType::Discovery) return std::nullopt;
        auto o = *parsed;
        DiscoveryPacket pkt;
        pkt.node_id = o.value("node_id").toString();
        pkt.protocol_version = o.value("protocol_version").toString();
        pkt.timestamp = o.value("timestamp").toVariant().toLongLong();
        pkt.data_port = static_cast<quint16>(o.value("data_port").toInt());
        pkt.udp_port = static_cast<quint16>(o.value("udp_port").toInt());
        pkt.tcp_port = static_cast<quint16>(o.value("tcp_port").toInt());
        // Topics
        const QJsonValue& topicsVal = o.value("topics");
        if (topicsVal.isArray()) {
            QJsonArray arr = topicsVal.toArray();
            for (const QJsonValue& v : arr) {
                if (v.isString()) pkt.topics << v.toString();
            }
        }
        // Serialization
        if (o.contains("serialization")) {
            const QJsonValue& serVal = o.value("serialization");
            if (serVal.isArray()) {
                QJsonArray arr = serVal.toArray();
                for (const QJsonValue& v : arr) {
                    if (v.isString()) pkt.serialization << v.toString();
                }
            }
        }
        return pkt;
    } else {
        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return std::nullopt;
        return from_json(doc.object());
    }
}

QByteArray Serializer::encodeAck(qint64 messageId, const QString& receiverId, const QString& status, qint64 ts, const QString& fmt) {
    if (fmt == "cbor") {
        return encodeAckCBOR(messageId, receiverId, status, ts);
    } else {
        return encodeAck(messageId, receiverId, status, ts);
    }
}

std::optional<QJsonObject> Serializer::decodeAck(const QByteArray& bytes, const QString& fmt) {
    if (fmt == "cbor") {
        PacketType t = PacketType::Unknown;
        auto parsed = decodeCBOR(bytes, &t);
        if (!parsed || t != PacketType::Ack) return std::nullopt;
        return *parsed;
    } else {
        PacketType t = PacketType::Unknown;
        auto parsed = decode(bytes, &t);
        if (!parsed || t != PacketType::Ack) return std::nullopt;
        return *parsed;
    }
}
QJsonObject Serializer::to_json(const DiscoveryPacket& pkt) {
    QJsonObject o;
    o["type"] = "discovery";
    o["node_id"] = pkt.node_id;
    o["protocol_version"] = pkt.protocol_version;
    o["timestamp"] = pkt.timestamp;
    o["topics"] = QJsonArray::fromStringList(pkt.topics);
    o["data_port"] = int(pkt.data_port);
    if (!pkt.serialization.isEmpty())
        o["serialization"] = QJsonArray::fromStringList(pkt.serialization);
    if (pkt.udp_port > 0)
        o["udp_port"] = int(pkt.udp_port);
    if (pkt.tcp_port > 0)
        o["tcp_port"] = int(pkt.tcp_port);
    return o;
}

std::optional<DiscoveryPacket> Serializer::from_json(const QJsonObject& o) {
    if (o.value("type").toString() != "discovery") return std::nullopt;
    if (!o.contains("node_id") || !o.contains("topics") || !o.contains("protocol_version") ||
        !o.contains("timestamp") || !o.contains("data_port")) {
        qWarning() << "[DROP][DISCOVERY] missing required field";
        return std::nullopt;
    }
    DiscoveryPacket pkt;
    pkt.node_id = o.value("node_id").toString();
    pkt.protocol_version = o.value("protocol_version").toString();
    pkt.timestamp = o.value("timestamp").toVariant().toLongLong();
    pkt.data_port = static_cast<quint16>(o.value("data_port").toInt());
    pkt.udp_port = static_cast<quint16>(o.value("udp_port").toInt());
    pkt.tcp_port = static_cast<quint16>(o.value("tcp_port").toInt());
    // Topics
    const QJsonValue& topicsVal = o.value("topics");
    if (topicsVal.isArray()) {
        QJsonArray arr = topicsVal.toArray();
        for (const QJsonValue& v : arr) {
            if (v.isString()) pkt.topics << v.toString();
        }
    }
    // Serialization
    if (o.contains("serialization")) {
        const QJsonValue& serVal = o.value("serialization");
        if (serVal.isArray()) {
            QJsonArray arr = serVal.toArray();
            for (const QJsonValue& v : arr) {
                if (v.isString()) pkt.serialization << v.toString();
            }
        }
    }
    return pkt;
}
