#include "tcp_transport.h"
#include "frame_codec.h"
#include <QLoggingCategory>
#include <QHostAddress>

Q_LOGGING_CATEGORY(lcTcp, "dds.net.tcp")

TcpTransport::TcpTransport(const Config &cfg, QObject *parent)
    : QObject(parent), cfg_(cfg) {
    server_.setMaxPendingConnections(128);
}

bool TcpTransport::start(){
    bool ok = true;
    if (cfg_.listen) {
        ok &= server_.listen(QHostAddress::AnyIPv4, cfg_.port);
        QObject::connect(&server_, &QTcpServer::newConnection, this, &TcpTransport::onNewConnection);
        qCInfo(lcTcp) << "TCP listening on" << cfg_.port << "ok=" << ok;
    }
    for (auto &hp : cfg_.connect){
        auto *s = new QTcpSocket(this);
        s->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, cfg_.rcvbuf);
        s->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, cfg_.sndbuf);
        s->connectToHost(hp.first, hp.second);
        if (!s->waitForConnected(cfg_.connectTimeoutMs)) {
            qCWarning(lcTcp) << "connect failed" << hp.first << hp.second;
            s->deleteLater();
            scheduleReconnect(hp.first, hp.second);
            continue;
        }
        attach(s);
    }
    return ok;
}

void TcpTransport::stop(){
    server_.close();
    for (auto *p : peers_) { if (p->sock) p->sock->close(); delete p; }
    peers_.clear();
    for (auto it = reconnectTimers_.begin(); it != reconnectTimers_.end(); ++it){
        it.value()->stop(); it.value()->deleteLater();
    }
    reconnectTimers_.clear();
}

void TcpTransport::onNewConnection(){
    while (server_.hasPendingConnections()) attach(server_.nextPendingConnection());
}

void TcpTransport::attach(QTcpSocket *s){
    auto *p = new Peer; p->sock = s; peers_ << p;
    QObject::connect(s, &QTcpSocket::readyRead, this, &TcpTransport::onReadyRead);
    QObject::connect(s, &QTcpSocket::disconnected, this, [this,s]{
        emit disconnected(s->peerAddress().toString());
        detach(s);
    });
    QObject::connect(s, qOverload<QAbstractSocket::SocketError>(&QTcpSocket::errorOccurred),
                     this, &TcpTransport::onSocketError);
    emit connected(s->peerAddress().toString());
}

void TcpTransport::detach(QTcpSocket *s){
    for (int i=0;i<peers_.size();++i){
        if (peers_[i]->sock == s){
            peers_[i]->sock->deleteLater();
            delete peers_[i];
            peers_.removeAt(i);
            break;
        }
    }
}

TcpTransport::Peer* TcpTransport::find(QTcpSocket *s){
    for (auto *p : peers_) if (p->sock == s) return p;
    return nullptr;
}

void TcpTransport::onReadyRead(){
    for (auto *p : peers_){
        if (!p->sock) continue;
        if (p->sock->bytesAvailable() > 0){
            p->rxBuf += p->sock->readAll();
            quint8 mt; QByteArray pl;
            while (dds::tryDecode(p->rxBuf, mt, pl)){
                emit frameReceived(mt, pl, p->sock->peerAddress().toString());
            }
        }
    }
}

bool TcpTransport::send(quint8 msgType, const QByteArray &payload){
    const QByteArray frame = dds::encode(msgType, payload);
    bool any=false;
    for (auto *p : peers_){
        if (p->sock && p->sock->state()==QTcpSocket::ConnectedState){
            p->sock->write(frame);
            any=true;
        }
    }
    return any;
}

void TcpTransport::onSocketError(QAbstractSocket::SocketError){
    auto *s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    const auto host = s->peerName().isEmpty() ? s->peerAddress().toString() : s->peerName();
    const auto port = s->peerPort();
    qCWarning(lcTcp) << "tcp error" << s->errorString() << host << port;
    // اگر outgoing بوده، reconnect ساده:
    scheduleReconnect(host, port);
}

void TcpTransport::scheduleReconnect(const QString &host, quint16 port){
    QString key = host + ":" + QString::number(port);
    auto *t = new QTimer(this);
    t->setInterval(cfg_.reconnectBackoffMs);
    t->setSingleShot(false);
    QObject::connect(t, &QTimer::timeout, this, [this,host,port,key,t]{
        int attempts = reconnectAttempts_.value(key, 0);
        if (attempts >= cfg_.maxReconnectAttempts) {
            qCWarning(lcTcp) << "[TCP][GIVEUP]" << host << port << "attempts=" << attempts;
            t->stop(); t->deleteLater();
            return;
        }
        reconnectAttempts_[key] = attempts + 1;
        auto* s = new QTcpSocket(this);
        s->connectToHost(host, port);
        if (s->waitForConnected(cfg_.connectTimeoutMs)) {
            reconnectAttempts_.remove(key);
            attach(s);
            t->stop(); t->deleteLater();
        } else {
            s->deleteLater(); // will retry on next tick
        }
    });
    t->start();
}
