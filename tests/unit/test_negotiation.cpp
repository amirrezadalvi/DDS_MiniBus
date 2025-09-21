#include <QTest>
#include <QJsonObject>
#include <QJsonArray>
#include "serialization/serializer.h"

class TestNegotiation : public QObject {
    Q_OBJECT

private slots:
    void testNegotiateFormatCBORPreferred() {
        QStringList ourPrefs = {"cbor", "json"};
        QStringList peerPrefs = {"cbor", "json"};
        QString result = Serializer::negotiateFormat(ourPrefs, peerPrefs);
        QCOMPARE(result, "cbor");
    }

    void testNegotiateFormatJSONPreferred() {
        QStringList ourPrefs = {"json", "cbor"};
        QStringList peerPrefs = {"json", "cbor"};
        QString result = Serializer::negotiateFormat(ourPrefs, peerPrefs);
        QCOMPARE(result, "json");
    }

    void testNegotiateFormatPeerOnlyJSON() {
        QStringList ourPrefs = {"cbor", "json"};
        QStringList peerPrefs = {"json"};
        QString result = Serializer::negotiateFormat(ourPrefs, peerPrefs);
        QCOMPARE(result, "json");
    }

    void testNegotiateFormatPeerOnlyCBOR() {
        QStringList ourPrefs = {"json", "cbor"};
        QStringList peerPrefs = {"cbor"};
        QString result = Serializer::negotiateFormat(ourPrefs, peerPrefs);
        QCOMPARE(result, "cbor");
    }

    void testNegotiateFormatNoCommon() {
        QStringList ourPrefs = {"cbor"};
        QStringList peerPrefs = {"json"};
        QString result = Serializer::negotiateFormat(ourPrefs, peerPrefs);
        QCOMPARE(result, "json"); // fallback
    }

    void testNegotiateFormatEmptyPeerPrefs() {
        QStringList ourPrefs = {"cbor", "json"};
        QStringList peerPrefs = {};
        QString result = Serializer::negotiateFormat(ourPrefs, peerPrefs);
        QCOMPARE(result, "cbor"); // our first preference
    }

    void testNegotiateFormatEmptyOurPrefs() {
        QStringList ourPrefs = {};
        QStringList peerPrefs = {"cbor", "json"};
        QString result = Serializer::negotiateFormat(ourPrefs, peerPrefs);
        QCOMPARE(result, "json"); // fallback
    }
};

QTEST_MAIN(TestNegotiation)
#include "test_negotiation.moc"