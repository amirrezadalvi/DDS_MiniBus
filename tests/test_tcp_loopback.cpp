#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include "transport/tcp_transport.h"
#include "transport/frame_codec.h"

using namespace dds; // برای MsgType

static const int kTotal = 1000;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // ---- سرور ----
    TcpTransport::Config srvCfg;
    srvCfg.listen = true;
    srvCfg.port = 38030;
    TcpTransport server(srvCfg);
    server.start();

    // ---- کلاینت ----
    TcpTransport::Config cliCfg;
    cliCfg.listen = false;
    cliCfg.connect = { {"127.0.0.1", 38030} };
    TcpTransport client(cliCfg);
    client.start();

    int sent = 0, acks = 0, receivedOnServer = 0;
    QElapsedTimer timer;

    // سرور وقتی DATA گرفت → ACK برگردونه
    QObject::connect(&server, &TcpTransport::frameReceived,
                     [&](quint8 mt, QByteArray pl, QString peer){
        if (mt == MsgType::DATA) {
            ++receivedOnServer;
            server.send(MsgType::ACK, pl);
        }
    });

    // کلاینت وقتی ACK گرفت → شمارش کن
    QObject::connect(&client, &TcpTransport::frameReceived,
                     [&](quint8 mt, QByteArray, QString){
        if (mt == MsgType::ACK) {
            ++acks;
            if (acks == kTotal) {
                qInfo() << "✅ All ACKs received:" << acks
                        << "elapsed(ms)" << timer.elapsed()
                        << "server saw DATA:" << receivedOnServer;
                QTimer::singleShot(100, &app, &QCoreApplication::quit);
            }
        }
    });

    // وقتی کلاینت وصل شد، شروع به ارسال کن
    QObject::connect(&client, &TcpTransport::connected, [&](QString){
        timer.start();
        QTimer* pump = new QTimer(&app);
        pump->setInterval(0); // در هر چرخه event
        QObject::connect(pump, &QTimer::timeout, [&](){
            for (int i=0; i<200 && sent<kTotal; ++i) {
                QByteArray payload; payload.setNum(sent);
                client.send(MsgType::DATA, payload);
                ++sent;
            }
            if (sent >= kTotal) pump->stop();
        });
        pump->start();
    });

    // تایم‌اوت ایمنی
    QTimer::singleShot(10000, &app, [&](){
        if (acks < kTotal) {
            qWarning() << "❌ Timeout: got" << acks << "ACKs of" << kTotal
                       << "server received:" << receivedOnServer;
            app.exit(1);
        }
    });

    return app.exec();
}
