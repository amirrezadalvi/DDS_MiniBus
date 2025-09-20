#pragma once
#include <QObject>
#include <QByteArray>
#include <QHostAddress>

class ITransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool send(const QByteArray& datagram, const QHostAddress& to, quint16 port) = 0;
    virtual quint16 boundPort() const = 0;
    virtual void stop() = 0;
signals:
    void datagramReceived(const QByteArray& bytes, QHostAddress from, quint16 port);
};
