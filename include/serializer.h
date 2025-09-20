#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QCborMap>
#include <QCborValue>
#include <QString>
#include <QStringList>
#include <optional>

enum class PacketType { Unknown, Discovery, Data, Ack };

struct DiscoveryPacket {
    QString node_id;
    QStringList topics;
    QString protocol_version;
    qint64 timestamp;
    quint16 data_port;
    QStringList serialization;
    quint16 udp_port = 0;
    quint16 tcp_port = 0;
};

struct MessageEnvelope {
    QString topic;
    qint64 message_id = 0;
    QJsonObject payload;
    qint64 timestamp = 0;
    QString qos;
    QString publisher_id;
};

namespace Serializer {
    QByteArray encodeDiscovery(const QString& nodeId, const QStringList& topics,
                                const QString& proto, qint64 ts, quint16 data_port,
                                const QStringList& serialization = QStringList());
    QByteArray encodeData(const MessageEnvelope& m);
    QByteArray encodeAck(qint64 messageId, const QString& receiverId, const QString& status, qint64 ts);
    std::optional<QJsonObject> decode(const QByteArray& bytes, PacketType* outType);

    // CBOR encoding functions
    QByteArray encodeDiscoveryCBOR(const QString& nodeId, const QStringList& topics,
                                    const QString& proto, qint64 ts, quint16 data_port,
                                    const QStringList& serialization = QStringList());
    QByteArray encodeDataCBOR(const MessageEnvelope& m);
    QByteArray encodeAckCBOR(qint64 messageId, const QString& receiverId, const QString& status, qint64 ts);
    std::optional<QJsonObject> decodeCBOR(const QByteArray& bytes, PacketType* outType);

    // Format negotiation
    QString negotiateFormat(const QStringList& ourPrefs, const QStringList& peerPrefs);
    // DiscoveryPacket helpers
    QJsonObject to_json(const DiscoveryPacket& pkt);
    std::optional<DiscoveryPacket> from_json(const QJsonObject& o);

    // Negotiation
    QString negotiateFormat(const QStringList& ourPrefs, const QStringList& peerPrefs);

    // Polymorphic encode/decode
    QByteArray encodeEnvelope(const MessageEnvelope& m, const QString& fmt);
    std::optional<MessageEnvelope> decodeEnvelope(const QByteArray& bytes, const QString& fmt);
    QByteArray encodeDiscovery(const DiscoveryPacket& pkt, const QString& fmt);
    std::optional<DiscoveryPacket> decodeDiscovery(const QByteArray& bytes, const QString& fmt);
    QByteArray encodeAck(qint64 messageId, const QString& receiverId, const QString& status, qint64 ts, const QString& fmt);
    std::optional<QJsonObject> decodeAck(const QByteArray& bytes, const QString& fmt);
}
