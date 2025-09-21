#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>

class TcpTransport : public QObject {
    Q_OBJECT
public:
    struct Config {
        bool listen = true;
        quint16 port = 38030;
        QList<QPair<QString,quint16>> connect; // {host,port}
        int rcvbuf = 262144, sndbuf = 262144;
        int connectTimeoutMs = 1500;
        int heartbeatMs = 0;            // 0 = disabled
        int reconnectBackoffMs = 500;   // simple fixed backoff
        int maxReconnectAttempts = 10;  // cap attempts
    };

    explicit TcpTransport(const Config &cfg, QObject *parent=nullptr);

    bool start();     // listen/connect طبق config
    void stop();

    // ارسال روی تمام سشن‌های متصل (ساده‌ترین مدل)
    bool send(quint8 msgType, const QByteArray &payload);

signals:
    void connected(QString peer);
    void disconnected(QString peer);
    void frameReceived(quint8 msgType, QByteArray payload, QString peer);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError);

private:
    struct Peer {
        QTcpSocket *sock=nullptr;
        QByteArray rxBuf;
    };

    Config cfg_;
    QTcpServer server_;
    QList<Peer*> peers_;
    QHash<QTcpSocket*, QTimer*> reconnectTimers_;
    QHash<QString, int> reconnectAttempts_;  // key: host:port

    void attach(QTcpSocket *s);
    void detach(QTcpSocket *s);
    void feed(Peer *p); // decode loop
    Peer* find(QTcpSocket *s);
    void scheduleReconnect(const QString &host, quint16 port);
};
