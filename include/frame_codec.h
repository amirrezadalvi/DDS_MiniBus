#pragma once
#include <QByteArray>
#include <QtEndian>

namespace dds {
enum MsgType : quint8 { DATA = 0x01, ACK = 0x02 };

inline QByteArray encode(quint8 msgType, const QByteArray &payload){
    quint32 len = 1u + payload.size(); // 1 byte msgType + payload
    QByteArray out; out.resize(4 + 1 + payload.size());
    qToBigEndian(len, reinterpret_cast<uchar*>(out.data())); // 4-byte length (BE)
    out[4] = static_cast<char>(msgType);
    if (!payload.isEmpty()) ::memcpy(out.data()+5, payload.constData(), payload.size());
    return out;
}

// returns true if one frame decoded; modifies buf by removing consumed bytes
inline bool tryDecode(QByteArray &buf, quint8 &msgType, QByteArray &payload){
    if (buf.size() < 5) return false;
    const quint32 len = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(buf.constData()));
    if (buf.size() < int(4 + len)) return false;
    msgType = static_cast<quint8>(buf[4]);
    payload = buf.mid(5, len-1);
    buf.remove(0, 4 + len);
    return true;
}
} // namespace dds
