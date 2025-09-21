#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCborValue>
#include <QCborMap>
#include "serialization/serializer.h"

class TestSerializer : public QObject {
    Q_OBJECT

private slots:
    void testValidData() {
        QJsonObject payload{{"temp", 25}, {"unit", "C"}};
        QJsonObject data{
            {"type", "data"},
            {"topic", "sensor/temperature"},
            {"message_id", 123},
            {"payload", payload},
            {"publisher_id", "node-1"},
            {"qos", "reliable"}
        };
        QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Data);
        QCOMPARE(result->value("topic").toString(), "sensor/temperature");
        QCOMPARE(result->value("message_id").toVariant().toLongLong(), 123LL);
    }

    void testValidAckMinimal() {
        QJsonObject ack{
            {"type", "ack"},
            {"message_id", 456}
        };
        QByteArray bytes = QJsonDocument(ack).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Ack);
        QCOMPARE(result->value("message_id").toVariant().toLongLong(), 456LL);
        QVERIFY(!result->contains("receiver_node_id") || result->value("receiver_node_id").toString().isEmpty());
    }

    void testValidAckWithReceiver() {
        QJsonObject ack{
            {"type", "ack"},
            {"message_id", 789},
            {"receiverId", "node-rx"}
        };
        QByteArray bytes = QJsonDocument(ack).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Ack);
        QCOMPARE(result->value("receiver_node_id").toString(), "node-rx");
    }

    void testValidDiscovery() {
        QJsonArray topics{"sensor/temp", "sensor/hum"};
        QJsonObject disc{
            {"type", "discovery"},
            {"node_id", "node-1"},
            {"topics", topics},
            {"data_port", 38020}
        };
        QByteArray bytes = QJsonDocument(disc).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Discovery);
        QCOMPARE(result->value("node_id").toString(), "node-1");
    }

    void testMalformedJson() {
        QByteArray bytes = "not json";
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(!result.has_value());
        QCOMPARE(pt, PacketType::Unknown);
    }

    void testMissingFieldsData() {
        QJsonObject data{
            {"type", "data"},
            {"topic", "sensor/temperature"},
            // missing message_id
            {"payload", QJsonObject{{"temp", 25}}},
            {"publisher_id", "node-1"},
            {"qos", "reliable"}
        };
        QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(!result.has_value());
    }

    void testMissingFieldsAck() {
        QJsonObject ack{
            {"type", "ack"}
            // missing message_id
        };
        QByteArray bytes = QJsonDocument(ack).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(!result.has_value());
    }

    void testMissingFieldsDiscovery() {
        QJsonObject disc{
            {"type", "discovery"},
            {"node_id", "node-1"},
            // missing topics
            {"data_port", 38020}
        };
        QByteArray bytes = QJsonDocument(disc).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(!result.has_value());
    }

    // CBOR tests
    void testValidDataCBOR() {
        QJsonObject payload{{"temp", 25}, {"unit", "C"}};
        MessageEnvelope msg{
            "sensor/temperature",
            123,
            payload,
            1234567890,
            "reliable",
            "node-1"
        };
        QByteArray bytes = Serializer::encodeDataCBOR(msg);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Data);
        QCOMPARE(result->value("topic").toString(), "sensor/temperature");
        QCOMPARE(result->value("message_id").toVariant().toLongLong(), 123LL);
        QCOMPARE(result->value("publisher_id").toString(), "node-1");
    }

    void testValidAckCBOR() {
        QByteArray bytes = Serializer::encodeAckCBOR(456, "node-rx", "received", 1234567890);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Ack);
        QCOMPARE(result->value("message_id").toVariant().toLongLong(), 456LL);
        QCOMPARE(result->value("receiver_node_id").toString(), "node-rx");
        QCOMPARE(result->value("status").toString(), "received");
    }

    void testValidDiscoveryCBOR() {
        QStringList topics{"sensor/temp", "sensor/hum"};
        QByteArray bytes = Serializer::encodeDiscoveryCBOR("node-1", topics, "1.0", 1234567890, 38020);
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Discovery);
        QCOMPARE(result->value("node_id").toString(), "node-1");
        QCOMPARE(result->value("data_port").toInt(), 38020);
    }

    void testMalformedCBOR() {
        QByteArray bytes = "not cbor";
        PacketType pt;
        auto result = Serializer::decode(bytes, &pt);
        QVERIFY(!result.has_value());
        QCOMPARE(pt, PacketType::Unknown);
    }

    void testAutoDetectCBOR() {
        // Test that CBOR is detected and decoded properly
        QJsonObject payload{{"temp", 25}, {"unit", "C"}};
        MessageEnvelope msg{
            "sensor/temperature",
            123,
            payload,
            1234567890,
            "reliable",
            "node-1"
        };
        QByteArray cborBytes = Serializer::encodeDataCBOR(msg);
        PacketType pt;
        auto result = Serializer::decode(cborBytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Data);
        QCOMPARE(result->value("topic").toString(), "sensor/temperature");
    }

    void testAutoDetectJSONFallback() {
        // Test that JSON is used as fallback when CBOR fails
        QJsonObject payload{{"temp", 25}, {"unit", "C"}};
        QJsonObject data{
            {"type", "data"},
            {"topic", "sensor/temperature"},
            {"message_id", 123},
            {"payload", payload},
            {"publisher_id", "node-1"},
            {"qos", "reliable"}
        };
        QByteArray jsonBytes = QJsonDocument(data).toJson(QJsonDocument::Compact);
        PacketType pt;
        auto result = Serializer::decode(jsonBytes, &pt);
        QVERIFY(result.has_value());
        QCOMPARE(pt, PacketType::Data);
        QCOMPARE(result->value("topic").toString(), "sensor/temperature");
    }

    // Round-trip tests
    void testRoundTripDataJSON() {
        QJsonObject payload{{"temp", 25.5}, {"unit", "C"}, {"active", true}};
        MessageEnvelope msg{"sensor/temp", 456, payload, 1234567890, "reliable", "node-test"};

        QByteArray encoded = Serializer::encodeData(msg);
        PacketType pt;
        auto decoded = Serializer::decode(encoded, &pt);

        QVERIFY(decoded.has_value());
        QCOMPARE(pt, PacketType::Data);
        QCOMPARE(decoded->value("topic").toString(), "sensor/temp");
        QCOMPARE(decoded->value("message_id").toVariant().toLongLong(), 456LL);
        QCOMPARE(decoded->value("publisher_id").toString(), "node-test");
        QCOMPARE(decoded->value("qos").toString(), "reliable");

        QJsonObject decodedPayload = decoded->value("payload").toObject();
        QCOMPARE(decodedPayload.value("temp").toDouble(), 25.5);
        QCOMPARE(decodedPayload.value("unit").toString(), "C");
        QCOMPARE(decodedPayload.value("active").toBool(), true);
    }

    void testRoundTripDataCBOR() {
        QJsonObject payload{{"temp", 25.5}, {"unit", "C"}, {"active", true}};
        MessageEnvelope msg{"sensor/temp", 456, payload, 1234567890, "reliable", "node-test"};

        QByteArray encoded = Serializer::encodeDataCBOR(msg);
        PacketType pt;
        auto decoded = Serializer::decode(encoded, &pt);

        QVERIFY(decoded.has_value());
        QCOMPARE(pt, PacketType::Data);
        QCOMPARE(decoded->value("topic").toString(), "sensor/temp");
        QCOMPARE(decoded->value("message_id").toVariant().toLongLong(), 456LL);
        QCOMPARE(decoded->value("publisher_id").toString(), "node-test");
        QCOMPARE(decoded->value("qos").toString(), "reliable");

        QJsonObject decodedPayload = decoded->value("payload").toObject();
        QCOMPARE(decodedPayload.value("temp").toDouble(), 25.5);
        QCOMPARE(decodedPayload.value("unit").toString(), "C");
        QCOMPARE(decodedPayload.value("active").toBool(), true);
    }

    void testRoundTripAckJSON() {
        QByteArray encoded = Serializer::encodeAck(789, "node-rx", "received", 1234567890);
        PacketType pt;
        auto decoded = Serializer::decode(encoded, &pt);

        QVERIFY(decoded.has_value());
        QCOMPARE(pt, PacketType::Ack);
        QCOMPARE(decoded->value("message_id").toVariant().toLongLong(), 789LL);
        QCOMPARE(decoded->value("receiver_node_id").toString(), "node-rx");
        QCOMPARE(decoded->value("status").toString(), "received");
    }

    void testRoundTripAckCBOR() {
        QByteArray encoded = Serializer::encodeAckCBOR(789, "node-rx", "received", 1234567890);
        PacketType pt;
        auto decoded = Serializer::decode(encoded, &pt);

        QVERIFY(decoded.has_value());
        QCOMPARE(pt, PacketType::Ack);
        QCOMPARE(decoded->value("message_id").toVariant().toLongLong(), 789LL);
        QCOMPARE(decoded->value("receiver_node_id").toString(), "node-rx");
        QCOMPARE(decoded->value("status").toString(), "received");
    }

    void testRoundTripDiscoveryJSON() {
        QStringList topics{"sensor/temp", "sensor/hum", "actuator/led"};
        QStringList serialization{"cbor", "json"};
        QByteArray encoded = Serializer::encodeDiscovery("node-test", topics, "1.0", 1234567890, 38020, serialization);
        PacketType pt;
        auto decoded = Serializer::decode(encoded, &pt);

        QVERIFY(decoded.has_value());
        QCOMPARE(pt, PacketType::Discovery);
        QCOMPARE(decoded->value("node_id").toString(), "node-test");
        QCOMPARE(decoded->value("data_port").toInt(), 38020);

        QJsonArray decodedTopics = decoded->value("topics").toArray();
        QCOMPARE(decodedTopics.size(), 3);
        QCOMPARE(decodedTopics[0].toString(), "sensor/temp");
        QCOMPARE(decodedTopics[1].toString(), "sensor/hum");
        QCOMPARE(decodedTopics[2].toString(), "actuator/led");

        QJsonArray decodedSer = decoded->value("serialization").toArray();
        QCOMPARE(decodedSer.size(), 2);
        QCOMPARE(decodedSer[0].toString(), "cbor");
        QCOMPARE(decodedSer[1].toString(), "json");
    }

    void testRoundTripDiscoveryCBOR() {
        QStringList topics{"sensor/temp", "sensor/hum", "actuator/led"};
        QStringList serialization{"cbor", "json"};
        QByteArray encoded = Serializer::encodeDiscoveryCBOR("node-test", topics, "1.0", 1234567890, 38020, serialization);
        PacketType pt;
        auto decoded = Serializer::decode(encoded, &pt);

        QVERIFY(decoded.has_value());
        QCOMPARE(pt, PacketType::Discovery);
        QCOMPARE(decoded->value("node_id").toString(), "node-test");
        QCOMPARE(decoded->value("data_port").toInt(), 38020);

        QJsonArray decodedTopics = decoded->value("topics").toArray();
        QCOMPARE(decodedTopics.size(), 3);
        QCOMPARE(decodedTopics[0].toString(), "sensor/temp");
        QCOMPARE(decodedTopics[1].toString(), "sensor/hum");
        QCOMPARE(decodedTopics[2].toString(), "actuator/led");

        QJsonArray decodedSer = decoded->value("serialization").toArray();
        QCOMPARE(decodedSer.size(), 2);
        QCOMPARE(decodedSer[0].toString(), "cbor");
        QCOMPARE(decodedSer[1].toString(), "json");
    }

    // Malformed frame tests
    void testMalformedCBORMap() {
        // Create invalid CBOR (not a map)
        QByteArray invalidCbor = QByteArray::fromHex("00"); // positive integer instead of map
        PacketType pt;
        auto result = Serializer::decode(invalidCbor, &pt);
        QVERIFY(!result.has_value());
        QCOMPARE(pt, PacketType::Unknown);
    }

    void testMalformedCBORNoType() {
        // Create CBOR map without "type" field
        QCborMap map;
        map[QCborValue("invalid")] = QCborValue("field");
        QByteArray invalidCbor = map.toCborValue().toCbor();
        PacketType pt;
        auto result = Serializer::decode(invalidCbor, &pt);
        QVERIFY(!result.has_value());
        QCOMPARE(pt, PacketType::Unknown);
    }

    void testMalformedJSON() {
        QByteArray invalidJson = "not json at all";
        PacketType pt;
        auto result = Serializer::decode(invalidJson, &pt);
        QVERIFY(!result.has_value());
        QCOMPARE(pt, PacketType::Unknown);
    }

    void testEmptyData() {
        QByteArray empty;
        PacketType pt;
        auto result = Serializer::decode(empty, &pt);
        QVERIFY(!result.has_value());
        QCOMPARE(pt, PacketType::Unknown);
    }
};

QTEST_MAIN(TestSerializer)
#include "test_serializer.moc"