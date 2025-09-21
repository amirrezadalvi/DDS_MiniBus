#include <QTest>
#include "serialization/serializer.h"

class TestAckPacket : public QObject {
    Q_OBJECT
private slots:
    void testAckJsonRoundtrip() {
        qint64 messageId = 12345;
        QString receiverId = "node-2";
        QString status = "ACK";
        qint64 ts = 1699999999;

        QByteArray encoded = Serializer::encodeAck(messageId, receiverId, status, ts);
        PacketType t = PacketType::Unknown;
        auto decoded = Serializer::decode(encoded, &t);
        QVERIFY(decoded.has_value());
        QCOMPARE(t, PacketType::Ack);

        QJsonObject obj = decoded.value();
        QCOMPARE(obj.value("type").toString(), QString("ack"));
        QCOMPARE(obj.value("message_id").toVariant().toLongLong(), messageId);
        QCOMPARE(obj.value("receiver_node_id").toString(), receiverId);
        QCOMPARE(obj.value("status").toString(), status);
        QCOMPARE(obj.value("timestamp").toVariant().toLongLong(), ts);
    }
};

QTEST_MAIN(TestAckPacket)
#include "test_ack_packet.moc"