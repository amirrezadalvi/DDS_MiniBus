#include "discovery_manager.h"
#include "serializer.h"
#include "config/config_manager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include "utils/logger.h"

DiscoveryManager::DiscoveryManager(const QString& nodeId, quint16 port, QObject* parent)
    : QObject(parent), nodeId(nodeId), port(port) {
    connect(&announceTimer, &QTimer::timeout, this, &DiscoveryManager::sendAnnouncement);
    connect(&expiryTimer, &QTimer::timeout, this, &DiscoveryManager::expirePeers);
}

void DiscoveryManager::start(bool announce) {
    if (!ConfigManager::ref().disc.enabled) {
        qInfo(LogDisc) << "discovery: disabled";
        return;
    }
    bindReceiver();
    if (announce) {
        announceTimer.start(intervalMs);
        QTimer::singleShot(10, this, &DiscoveryManager::sendAnnouncement);
    }
    expiryTimer.start(intervalMs);
}

void DiscoveryManager::bindReceiver() {
    QHostAddress bindAddr = loopbackMode ? QHostAddress::LocalHost : QHostAddress::AnyIPv4;
    if (!socket.bind(bindAddr, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning(LogDisc) << "discovery: Bind failed on" << port << ":" << socket.errorString();
    }
    if (mode == "multicast" && !loopbackMode) {
        if (!socket.joinMulticastGroup(mcastAddr)) {
            qWarning(LogDisc) << "discovery: joinMulticastGroup failed for" << mcastAddr.toString();
        }
        socket.setSocketOption(QAbstractSocket::MulticastTtlOption, ConfigManager::ref().disc.ttl);
    }
    connect(&socket, &QUdpSocket::readyRead, this, &DiscoveryManager::processPendingDatagrams);
}

void DiscoveryManager::sendAnnouncement() {
    const auto supported = getSupportedSerialization();
    const auto& cfg = ConfigManager::ref();
    DiscoveryPacket pkt;
    pkt.node_id = nodeId;
    pkt.topics = topics;
    pkt.protocol_version = protoVersion;
    pkt.timestamp = QDateTime::currentSecsSinceEpoch();
    pkt.data_port = dataPort;
    pkt.serialization = supported;
    pkt.udp_port = dataPort;  // Use actual bound port for data
    pkt.tcp_port = cfg.transport.tcp.port;
    QByteArray datagram = Serializer::encodeDiscovery(pkt, "json"); // Use JSON for discovery
    if (loopbackMode) {
        socket.writeDatagram(datagram, QHostAddress::LocalHost, port);
    } else if (mode == "multicast") {
        socket.writeDatagram(datagram, mcastAddr, port);
    } else {
        socket.writeDatagram(datagram, QHostAddress::Broadcast, port);
    }
    qCDebug(LogDisc) << "discovery: announce node=" << nodeId << "disc=" << port << "udp=" << pkt.udp_port << "tcp=" << pkt.tcp_port << "formats=" << supported << (loopbackMode ? " [LOOPBACK]" : "");
}

QStringList DiscoveryManager::getSupportedSerialization() const {
    return ConfigManager::ref().serialization.supported;
}

void DiscoveryManager::processPendingDatagrams() {
    while (socket.hasPendingDatagrams()) {
        QByteArray d; d.resize(int(socket.pendingDatagramSize()));
        QHostAddress from; quint16 p=0;
        socket.readDatagram(d.data(), d.size(), &from, &p);
        auto pktOpt = Serializer::decodeDiscovery(d, "json"); // Decode as JSON for discovery
        if (!pktOpt.has_value()) continue;
        const DiscoveryPacket& pkt = pktOpt.value();
        if (pkt.node_id.isEmpty() || pkt.node_id == nodeId) continue;
        PeerInfo info;
        info.node_id = pkt.node_id;
        info.topics = pkt.topics;
        info.proto_version = pkt.protocol_version;
        info.last_seen = QDateTime::currentSecsSinceEpoch();
        info.transport_hint = from.toString();
        info.serialization_formats = pkt.serialization;
        info.udp_port = pkt.udp_port;
        info.tcp_port = pkt.tcp_port;
        {
            QMutexLocker locker(&peerMutex);
            peerTable[info.node_id] = info;
        }
        emit peerUpdated(info.node_id, Serializer::to_json(pkt));
        qCInfo(LogDisc) << "discovery: peer=" << info.node_id << "topics=" << info.topics.size() << "(ver=" << info.proto_version << ", formats=" << info.serialization_formats << ")";
    }
}

void DiscoveryManager::stop() {
    announceTimer.stop();
    expiryTimer.stop();
    socket.close();
}

QVector<PeerInfo> DiscoveryManager::list_peers() const {
    QMutexLocker locker(&peerMutex);
    return peerTable.values().toVector();
}

bool DiscoveryManager::has_peer(const QString& node_id) const {
    QMutexLocker locker(&peerMutex);
    return peerTable.contains(node_id);
}

PeerInfo DiscoveryManager::get_peer(const QString& node_id) const {
    QMutexLocker locker(&peerMutex);
    return peerTable.value(node_id);
}

QStringList DiscoveryManager::topics_for(const QString& node_id) const {
    QMutexLocker locker(&peerMutex);
    if (peerTable.contains(node_id)) return peerTable[node_id].topics;
    return {};
}
QStringList DiscoveryManager::serialization_formats_for(const QString& node_id) const {
    QMutexLocker locker(&peerMutex);
    if (peerTable.contains(node_id)) return peerTable[node_id].serialization_formats;
    return {};
}

void DiscoveryManager::expirePeers() {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const int expiry = 10; // 10 seconds for demo runs
    QMutexLocker locker(&peerMutex);
    QList<QString> toRemove;
    for (auto it = peerTable.begin(); it != peerTable.end(); ++it) {
        if (now - it.value().last_seen > expiry) {
            toRemove << it.key();
            qInfo(LogDisc) << "discovery: expired peer=" << it.key() << "(age=" << (now - it.value().last_seen) << "s)";
        }
    }
    for (const auto& k : toRemove) peerTable.remove(k);
}
