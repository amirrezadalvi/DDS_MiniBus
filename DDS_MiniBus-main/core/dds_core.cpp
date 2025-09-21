#include "dds_core.h"
#include "publisher.h"
#include "qos.h"
#include "transport/ack_manager.h"
#include "config/config_manager.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QStringList>
#include <QSet>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMap>




DDSCore::DDSCore(const QString& nodeId, const QString& proto,
                  ITransport* transport, AckManager* ackMgr, QObject* parent)
  : QObject(parent),
    node_id(nodeId),
    protocol(proto),
    net(transport),
    ack(ackMgr),
    seenMessages(ConfigManager::ref().qos_cfg.dedup_capacity)
    , perTopicDedup()
    , discoveryManager(nullptr)
{
    connect(net, &ITransport::datagramReceived, this, &DDSCore::onDatagram);
    if (ack) {
        connect(ack, &AckManager::resend, this, &DDSCore::resendPacket);
        connect(ack, &AckManager::failed, this, &DDSCore::onAckFailed);
    }
}

class Publisher DDSCore::makePublisher(const QString& topic) {
    if (!topics.contains(topic)) { TopicInfo t; t.name = topic; topics.insert(topic, t); }
    // Log peer count for this topic
    if (discoveryManager) {
        int count = 0;
        for (const auto& peer : discoveryManager->list_peers()) {
            if (peer.topics.contains(topic)) ++count;
        }
        qInfo(LogDisc) << "makePublisher: topic=" << topic << "peers advertising this topic:" << count;
    }
    return Publisher(*this, topic);
}

// --- makeSubscriber ---
class Subscriber DDSCore::makeSubscriber(const QString& topic, Subscriber::Callback cb) {
    Subscriber s(*this, topic, cb);
    // فقط callback را ذخیره می‌کنیم
    subs.insert(topic, cb);

    if (!topics.contains(topic)) { TopicInfo t; t.name = topic; topics.insert(topic, t); }
    topics[topic].subscribers << "local";
    // Log peer count for this topic
    if (discoveryManager) {
        int count = 0;
        for (const auto& peer : discoveryManager->list_peers()) {
            if (peer.topics.contains(topic)) ++count;
        }
        qInfo(LogDisc) << "makeSubscriber: topic=" << topic << "peers advertising this topic:" << count;
    }
    if (lastMsg.contains(topic) && cb) {
        QJsonObject enriched = lastMsg.value(topic);
        enriched["topic"] = topic;
        enriched["qos"] = "best_effort"; // or something, but since it's lastMsg, perhaps not needed
        enriched["message_id"] = 0; // placeholder
        cb(enriched);
    }
    deliverRetainLast(topic, ConfigManager::ref().node_id);
    return s;
}


qint64 DDSCore::publishInternal(const QString& topic, const QJsonObject& payload, const QString& qos) {
    MessageEnvelope m; m.topic=topic; m.payload=payload; m.qos=qos; m.publisher_id=node_id;
    m.message_id = next_msg_id++; m.timestamp = QDateTime::currentSecsSinceEpoch();
    const bool reliable = isReliable(qos);
    sendMessage(m, reliable); lastMsg.insert(topic, payload);
    if (ConfigManager::ref().qos_cfg.retain_last) {
        last_by_topic_[topic] = m;
    }
    return m.message_id;
}

void DDSCore::sendMessage(const MessageEnvelope& m, bool reliable) {
    const auto& cfg = ConfigManager::ref();
    const QString ourFormat = cfg.serialization.format;
    const QStringList ourPrefs = cfg.serialization.supported;

    QStringList destPeers;
    if (discoveryManager) {
        for (const auto& peer : discoveryManager->list_peers()) {
            if (peer.topics.contains(m.topic)) destPeers << peer.node_id;
        }
    } else {
        // Fallback to old peers map
        for (auto it = peers.begin(); it != peers.end(); ++it) {
            const QString pid = it.key();
            const QJsonObject o = it.value();
            bool has = false;
            const QJsonValue topicsVal = o.value("topics");
            if (topicsVal.isArray()) {
                const QJsonArray arr = topicsVal.toArray();
                for (const QJsonValue& v : arr) {
                    if (v.isString() && v.toString() == m.topic) { has = true; break; }
                }
            }
            if (has) destPeers << pid;
        }
    }

    if (reliable) {
        if (destPeers.isEmpty()) {
            qCWarning(LogNet) << "[ROUTE][MISS] no peers for reliable topic=" << m.topic << "; dropping mid=" << m.message_id;
            return;
        }
        for (const auto& pid : destPeers) {
            QString negotiatedFormat = peerFormats.value(pid, "");
            if (negotiatedFormat.isEmpty()) {
                // Negotiate format with peer
                QStringList peerPrefs;
                if (discoveryManager && discoveryManager->has_peer(pid)) {
                    peerPrefs = discoveryManager->serialization_formats_for(pid);
                } else {
                    const QJsonObject o = peers.value(pid);
                    const QJsonValue serVal = o.value("serialization");
                    if (serVal.isArray()) {
                        const QJsonArray arr = serVal.toArray();
                        for (const QJsonValue& v : arr) {
                            if (v.isString()) peerPrefs << v.toString();
                        }
                    }
                }
                negotiatedFormat = Serializer::negotiateFormat(ourPrefs, peerPrefs);
                if (negotiatedFormat.isEmpty()) {
                    if (cfg.serialization.allow_json_fallback) {
                        negotiatedFormat = "json";
                        qCWarning(LogNet) << "[NEGOTIATE][FALLBACK] no mutual format with " << pid << ", using json";
                    } else {
                        qCritical(LogNet) << "[NEGOTIATE][FAIL] no mutual format with " << pid << ", skipping";
                        continue;
                    }
                }
                peerFormats[pid] = negotiatedFormat;
                qCInfo(LogNet) << "[NEGOTIATE] chosen=" << negotiatedFormat << " local=" << ourPrefs << " remote=" << peerPrefs;
            }

            // Get peer address
            QString ip = "127.0.0.1"; // Assume localhost for now
            quint16 dp = 0;
            if (discoveryManager && discoveryManager->has_peer(pid)) {
                PeerInfo peerInfo = discoveryManager->get_peer(pid);
                dp = peerInfo.udp_port;
            } else {
                const QJsonObject o = peers.value(pid);
                dp = static_cast<quint16>(o.value("data_port").toInt(net->boundPort()));
            }
            if (dp == 0) continue;

            qCInfo(LogNet) << "[ROUTE] topic=" << m.topic << " peer=" << pid << " -> udp=" << ip << ":" << dp;
        
            try {
                // Encode packet in negotiated format
                QByteArray packet = Serializer::encodeEnvelope(m, negotiatedFormat);
                qCDebug(LogNet) << "[SEND][ENVELOPE] size=" << packet.size() << " fmt=" << negotiatedFormat << " topic=" << m.topic << " mid=" << m.message_id << " peers=" << destPeers.size();
        
                int bytesSent = net->send(packet, QHostAddress(ip), dp);
                qCDebug(LogNet) << "[SEND][UNICAST] mid=" << m.message_id << " -> " << ip << ":" << dp << " bytes=" << bytesSent;
        
                if (ack) {
                    auto& cfg = ConfigManager::ref();
                    Pending p;
                    p.packet = packet;
                    p.retries_left = cfg.qos_cfg.reliable.max_retries;
                    p.deadline_ms = QDateTime::currentMSecsSinceEpoch() + cfg.qos_cfg.reliable.ack_timeout_ms;
                    p.base_timeout_ms = cfg.qos_cfg.reliable.ack_timeout_ms;
                    p.exponential_backoff = cfg.qos_cfg.reliable.exponential_backoff;
                    p.to = QHostAddress(ip);
                    p.port = dp;
                    p.msg_id = m.message_id;
                    p.receiver_id = pid;
                    ack->track(p);
                    qCDebug(LogQoS) << "[TRACK]" << m.message_id << "to" << pid;
                }
            } catch (const std::exception& e) {
                qCritical(LogNet) << "[SEND][EXC] mid=" << m.message_id << " to " << pid << " what=" << e.what();
            } catch (...) {
                qCritical(LogNet) << "[SEND][EXC] mid=" << m.message_id << " to " << pid << " unknown";
            }
        }
        qCDebug(LogNet) << "[SEND][DONE] mid=" << m.message_id << " sent to " << destPeers.size() << " peers";
    } else {
        // best-effort: broadcast (use our preferred format)
        QByteArray packet = Serializer::encodeEnvelope(m, ourFormat);
        net->send(packet, QHostAddress::Broadcast, ConfigManager::ref().transport.udp.port);
        qCDebug(LogNet) << "[SEND][BCAST]" << m.topic << "mid=" << m.message_id << "(fmt=" << ourFormat << ")";
    }
}

// --- deliverToLocal ---
void DDSCore::deliverToLocal(const QString& topic, const QJsonObject& payload, const QString& qos, qint64 msg_id) {
    QJsonObject enriched = payload;
    enriched["topic"] = topic;
    enriched["qos"] = qos;
    enriched["message_id"] = msg_id;
    if (subs.contains(topic)) {
        auto cb = subs.value(topic);
        if (cb) cb(enriched);
    }
    lastMsg.insert(topic, enriched);
}


void DDSCore::onDatagram(const QByteArray& bytes, QHostAddress from, quint16 port) {
    qCDebug(LogNet) << "[UDP-IN] src=" << from.toString() << ":" << port << " len=" << bytes.size();
    PacketType t = PacketType::Unknown; auto parsed = Serializer::decode(bytes, &t); if (!parsed) return;
    auto o = *parsed;
    if (t == PacketType::Data) {
        const auto topic = o.value("topic").toString();
        const auto publisher = o.value("publisher_id").toString(); if (publisher == node_id) return;
        const qint64 mid = o.value("message_id").toVariant().toLongLong();
        // Per-topic LRU/set for deduplication
        QSet<qint64>& topicSet = perTopicDedup[topic];
        if (topicSet.contains(mid)) {
            qCDebug(LogNet) << "[DUP][PER-TOPIC] skipping" << topic << mid;
            return;
        }
        topicSet.insert(mid);
        // Bounded to dedup_capacity
        if (topicSet.size() > ConfigManager::ref().qos_cfg.dedup_capacity) {
            // Remove oldest (not strictly LRU, but sufficient for bounded set)
            auto it = topicSet.begin();
            topicSet.erase(it);
        }
        const QString key = publisher + ":" + topic + ":" + QString::number(mid);
        if (seenMessages.contains(key)) {
            qCDebug(LogNet) << "[DUP] skipping" << key;
            return;
        }
        seenMessages.insert(key);
        const auto qos = o.value("qos").toString();
        deliverToLocal(topic, o.value("payload").toObject(), qos, mid);
        if (isReliable(qos)) {
            const qint64 mid = o.value("message_id").toVariant().toLongLong();

            // Find the peer and negotiate format for ACK
            QString ackFormat = "json"; // default fallback
            for (auto it = peers.begin(); it != peers.end(); ++it) {
                const QJsonObject peerObj = it.value();
                if (peerObj.value("sender_ip").toString() == from.toString() &&
                    static_cast<quint16>(peerObj.value("data_port").toInt()) == port) {
                    QStringList peerPrefs;
                    const QJsonValue serVal = peerObj.value("serialization");
                    if (serVal.isArray()) {
                        const QJsonArray arr = serVal.toArray();
                        for (const QJsonValue& v : arr) {
                            if (v.isString()) peerPrefs << v.toString();
                        }
                    }
                    ackFormat = Serializer::negotiateFormat(ConfigManager::ref().serialization.supported, peerPrefs);
                    break;
                }
            }

            qCDebug(LogQoS) << "[ACK][OUT] mid=" << mid << " fmt=json" << " to=" << from.toString() << ":" << port;

            // Always send ACK in JSON for simplicity
            QByteArray ackPkt = Serializer::encodeAck(mid, node_id, "ACK", QDateTime::currentSecsSinceEpoch());

            net->send(ackPkt, from, port);
            qCDebug(LogQoS) << "[ACK][TX]" << mid << "->" << from.toString() << ":" << port << "(fmt=" << ackFormat << ")";
        }
    } else if (t == PacketType::Ack) {
        if (ack) {
            const qint64 mid = o.value("message_id").toVariant().toLongLong();
            const QString receiverId = o.value("receiver_node_id").toString();
            qCDebug(LogQoS) << "[ACK][IN] mid=" << mid << " fmt=json bytes=" << bytes.size();
            ack->ackReceived(mid, receiverId);
            qCDebug(LogQoS) << "[ACK][RX]" << mid << "from" << receiverId;
        }
    }
}

void DDSCore::updatePeers(const QString& peerId, const QJsonObject& payload) { peers.insert(peerId, payload); }
QStringList DDSCore::advertisedTopics() const { return topics.keys(); }

void DDSCore::resendPacket(const Pending& p) {
    qCDebug(LogQoS) << "[RESEND] mid=" << p.msg_id << " to=" << p.to.toString() << ":" << p.port << " attempt=" << p.attempt << " size=" << p.packet.size();
    net->send(p.packet, p.to, p.port);
}

QVector<PeerInfo> DDSCore::get_known_peers() const {
    if (discoveryManager) return discoveryManager->list_peers();
    return {};
}

void DDSCore::onAckFailed(qint64 msg_id, const QString& receiverId) {
    qCWarning(LogQoS) << "[DEADLETTER]" << msg_id << "from" << receiverId;
    // Could emit signal or handle dead letter
}

void DDSCore::shutdown(int timeoutMs) {
    if (!ack) {
        if (net) net->stop();
        return;
    }

    QElapsedTimer timer;
    timer.start();
    while (ack->hasPending() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10); // wait a bit
    }

    if (net) net->stop();
}

void DDSCore::deliverRetainLast(const QString& topic, const QString& receiverNodeId) {
    if (!ConfigManager::ref().qos_cfg.retain_last) return;
    if (!last_by_topic_.contains(topic)) return;
    const MessageEnvelope m = last_by_topic_.value(topic);
    // Deliver to local subscribers
    deliverToLocal(topic, m.payload, m.qos, m.message_id);
}
