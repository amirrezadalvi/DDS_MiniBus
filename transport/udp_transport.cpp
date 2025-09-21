#include "udp_transport.h"
#include <QDebug>

UdpTransport::UdpTransport(quint16 bindPort, QObject* parent): ITransport(parent) {
    if (!sock.bind(QHostAddress::AnyIPv4, bindPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning() << "[UDP][BIND][FAIL] port=" << bindPort << " reason=" << sock.errorString();
        // Try ephemeral port
        if (!sock.bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            qCritical() << "[UDP][BIND][FAIL] ephemeral port also failed";
            // Exit or handle error
        } else {
            qInfo() << "[UDP][BIND][EPHEMERAL] port=" << sock.localPort();
        }
    } else {
        qInfo() << "[UDP][BIND][OK] port=" << bindPort;
    }
    connect(&sock, &QUdpSocket::readyRead, this, &UdpTransport::onReadyRead);
}

bool UdpTransport::send(const QByteArray& datagram, const QHostAddress& to, quint16 port) {
    return sock.writeDatagram(datagram, to, port) == datagram.size();
}

void UdpTransport::onReadyRead() {
    while (sock.hasPendingDatagrams()) {
        QByteArray d; d.resize(int(sock.pendingDatagramSize()));
        QHostAddress from; quint16 p = 0;
        sock.readDatagram(d.data(), d.size(), &from, &p);
        emit datagramReceived(d, from, p);
    }
}

quint16 UdpTransport::boundPort() const { return sock.localPort(); }

void UdpTransport::stop() { sock.close(); }
