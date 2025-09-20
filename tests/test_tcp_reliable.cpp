#include <QCoreApplication>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include "transport/tcp_transport.h"
#include "transport/frame_codec.h"

// Test TCP transport functionality directly
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qInfo() << "[TEST] Starting TCP transport functionality test";

    // Setup TCP server transport
    TcpTransport::Config serverConfig{
        true, 38040, {}, 262144, 262144, 1500, 0, 500, 10
    };

    // Setup TCP client transport
    TcpTransport::Config clientConfig{
        false, 0, {{"127.0.0.1", 38040}}, 262144, 262144, 1500, 0, 500, 10
    };

    TcpTransport* server = new TcpTransport(serverConfig);
    TcpTransport* client = new TcpTransport(clientConfig);

    bool messageReceived = false;
    QString receivedData;
    quint8 receivedMsgType = 0;

    // Connect server signals
    QObject::connect(server, &TcpTransport::frameReceived,
                     [&](quint8 msgType, const QByteArray& payload, const QString& peer) {
        receivedMsgType = msgType;
        QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (doc.isObject()) {
            receivedData = doc.object().value("data").toString();
            messageReceived = true;
            qInfo() << "[TEST] Server received message:" << receivedData << "from" << peer;
        }
    });

    // Start server
    if (!server->start()) {
        qCritical() << "[TEST] Failed to start TCP server";
        return 1;
    }

    // Start client
    if (!client->start()) {
        qCritical() << "[TEST] Failed to start TCP client";
        return 1;
    }

    // Send test message after connection delay
    QTimer::singleShot(1500, [&]() {
        qInfo() << "[TEST] Sending test message over TCP";

        // Create test message
        QJsonObject testMsg{{"data", "tcp-transport-test"}};
        QByteArray payload = QJsonDocument(testMsg).toJson(QJsonDocument::Compact);

        // Send using frame encoding
        if (client->send(1, payload)) {  // msgType = 1 (DATA)
            qInfo() << "[TEST] Message sent successfully";
        } else {
            qCritical() << "[TEST] Failed to send message";
        }
    });

    // Check result after 3 seconds
    QTimer::singleShot(3000, [&]() {
        if (messageReceived && receivedData == "tcp-transport-test" && receivedMsgType == 1) {
            qInfo() << "[TEST] ✅ TCP transport test PASSED";
            qInfo() << "[TEST] Message type:" << receivedMsgType << "Data:" << receivedData;
            app.exit(0);
        } else {
            qCritical() << "[TEST] ❌ TCP transport test FAILED";
            qCritical() << "[TEST] Received:" << receivedData << "Expected: tcp-transport-test";
            qCritical() << "[TEST] Message type:" << receivedMsgType << "Expected: 1";
            app.exit(1);
        }
    });

    return app.exec();
}