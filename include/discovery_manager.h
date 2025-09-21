#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QHash>
#include <QJsonObject>
#include <QStringList>

#include <QMutex>
#include <QVector>
#include <chrono>

#include <QMutex>
#include <QVector>
#include <chrono>

struct PeerInfo {
    QString node_id;
    QStringList topics;
    QString proto_version;
    qint64 last_seen; // unix epoch seconds
    QString transport_hint;
    QStringList serialization_formats;
    quint16 udp_port = 0;
    quint16 tcp_port = 0;
};

class DiscoveryManager : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryManager(const QString& nodeId, quint16 port, QObject* parent = nullptr);
    void setProtocolVersion(const QString& v) { protoVersion = v; }
    void setIntervalMs(int ms) {
        intervalMs = ms;
        if (announceTimer.isActive()) {
            announceTimer.start(intervalMs);
        }
    }
    void setMode(const QString& m) { mode = m.toLower(); }
    void setMulticastAddress(const QHostAddress& a) { mcastAddr = a; }
    void setLoopbackMode(bool enable) { loopbackMode = enable; }
    void setAdvertisedTopics(const QStringList& t) { topics = t; }
    void setDataPort(quint16 p) { dataPort = p; }
    QVector<PeerInfo> list_peers() const;
    bool has_peer(const QString& node_id) const;
    PeerInfo get_peer(const QString& node_id) const;
    QStringList topics_for(const QString& node_id) const;
    QStringList serialization_formats_for(const QString& node_id) const;
    QStringList getSupportedSerialization() const;

    void start(bool announce);
    void stop();

signals:
    void peerUpdated(const QString& nodeId, const QJsonObject& payload);

private slots:
    void sendAnnouncement();
    void processPendingDatagrams();

private:
    void bindReceiver();
    void expirePeers();

    QString nodeId;
    quint16 port;
    QUdpSocket socket;
    QTimer announceTimer;
    QTimer expiryTimer;

    QString protoVersion = "1.0";
    int intervalMs = 1000;
    QString mode = "broadcast";
    QHostAddress mcastAddr = QHostAddress("239.255.0.1");
    QStringList topics;
    quint16 dataPort = 0;
    bool loopbackMode = false;

    mutable QMutex peerMutex;
    QHash<QString, PeerInfo> peerTable;
};
