#pragma once
#include "transport/transport_base.h"
#include <QUdpSocket>

class UdpTransport : public ITransport {
    Q_OBJECT
public:
    explicit UdpTransport(quint16 bindPort, QObject* parent=nullptr);
    bool send(const QByteArray& datagram, const QHostAddress& to, quint16 port) override;
    quint16 boundPort() const override;
    void stop() override;
private slots:
    void onReadyRead();
private:
    QUdpSocket sock;
};
