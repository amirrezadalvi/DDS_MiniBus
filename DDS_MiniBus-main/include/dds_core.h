#pragma once
#include <QObject>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QVector>
#include <QStringList>

#include "serializer.h"
#include "transport_base.h"
#include "ack_manager.h"
#include "topic.h"
#include "subscriber.h"
#include "bounded_lru.h"
#include "logger.h"
#include "discovery_manager.h"

class Publisher;    // fwd (تعریف در publisher.h)

class DDSCore : public QObject {
    Q_OBJECT
public:
    DDSCore(const QString& nodeId, const QString& proto,
            ITransport* transport, AckManager* ack, QObject* parent=nullptr);

    class Publisher makePublisher(const QString& topic);
    class Subscriber makeSubscriber(const QString& topic, Subscriber::Callback cb);

    void onDatagram(const QByteArray& bytes, QHostAddress from, quint16 port);
    void updatePeers(const QString& peerId, const QJsonObject& payload);
    QStringList advertisedTopics() const;
    void deliverToLocal(const QString& topic, const QJsonObject& payload, const QString& qos, qint64 msg_id);

    void shutdown(int timeoutMs = 500);

    // Discovery peer query
    QVector<PeerInfo> get_known_peers() const;

    qint64 publishInternal(const QString& topic, const QJsonObject& payload, const QString& qos);

private slots:
    void resendPacket(const Pending& p);
    void onAckFailed(qint64 msg_id, const QString& receiverId);

private:
    void sendMessage(const MessageEnvelope& m, bool reliable);

    QString node_id;
    QString protocol;
    ITransport* net = nullptr;
    AckManager* ack = nullptr;

    QHash<QString, TopicInfo>     topics;
    QHash<QString, Subscriber::Callback> subs;   // ← فقط callback نگه می‌داریم
    QHash<QString, QJsonObject>   peers;
    QHash<QString, QJsonObject>   lastMsg;
    BoundedLRU                    seenMessages;  // for de-duplication
    qint64 next_msg_id = 1;
    QMap<QString, QSet<qint64>> perTopicDedup;
    DiscoveryManager* discoveryManager = nullptr;
    QHash<QString, QString> peerFormats; // node_id -> chosen format
    QHash<QString, MessageEnvelope> last_by_topic_; // for retain_last

public:
    void setDiscoveryManager(DiscoveryManager* dm) { discoveryManager = dm; }
    void deliverRetainLast(const QString& topic, const QString& receiverNodeId);
};
