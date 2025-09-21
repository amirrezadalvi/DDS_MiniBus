#pragma once
#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QHash>
#include <QTimer>
#include <QVector>

struct Pending {
    QByteArray packet;
    int retries_left = 0;
    qint64 deadline_ms = 0;
    qint64 base_timeout_ms = 200;
    int attempt = 0;
    bool exponential_backoff = true;
    QHostAddress to;
    quint16 port = 0;
    qint64 msg_id = 0;
    QString receiver_id;
};

struct DeadLetter {
    qint64 msg_id;
    QString receiver_id;
    QByteArray packet;
    qint64 failed_at_ms;
};

class AckManager : public QObject {
    Q_OBJECT
public:
    explicit AckManager(QObject* parent=nullptr);
    void track(const Pending& p);
    void ackReceived(qint64 msg_id, const QString& receiverId);
    bool hasPending() const { return !pending.isEmpty(); }
    const QVector<DeadLetter>& deadLetters() const { return dead_letters; }
    int deadLetterSize() const { return dead_letters.size(); }
    int ackCount() const { return ack_count; }
signals:
    void resend(const Pending& p);
    void failed(qint64 msg_id, const QString& receiverId);
    void deadLetter(qint64 messageId, QString receiverId, int attempts, QString reason);
private slots:
    void onTick();
private:
    static QString makeKey(qint64 msg_id, const QString& receiverId);
    void appendDeadLetter(qint64 id, const QString& rx, int attempts, const QString& reason);
    QHash<QString, Pending> pending;
    QVector<DeadLetter> dead_letters;
    QTimer timer;
    int ack_count = 0;
};
