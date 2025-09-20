#include "ack_manager.h"
#include "config_manager.h"
#include <QDateTime>
#include <QtGlobal>
#include <QFile>
#include <QDir>
#include <QFileInfo>

static inline qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

AckManager::AckManager(QObject* parent): QObject(parent) {
    connect(&timer, &QTimer::timeout, this, &AckManager::onTick);
    timer.start(30);
}

QString AckManager::makeKey(qint64 msg_id, const QString& receiverId) {
    return QString::number(msg_id) + ":" + receiverId;
}

void AckManager::track(const Pending& p) {
    pending.insert(makeKey(p.msg_id, p.receiver_id), p);
}

void AckManager::ackReceived(qint64 msg_id, const QString& receiverId) {
    pending.remove(makeKey(msg_id, receiverId));
    ack_count++;
}

void AckManager::onTick() {
    const qint64 now = nowMs();
    QList<QString> toRemove;
    QList<QString> toUpdate;
    QHash<QString, Pending> updates;

    for (auto it = pending.begin(); it != pending.end(); ++it) {
        auto p = it.value();
        if (now >= p.deadline_ms) {
            if (p.retries_left > 0) {
                p.attempt += 1;
                p.retries_left -= 1;
                qint64 next = p.base_timeout_ms;
                if (p.exponential_backoff) next = p.base_timeout_ms * (1LL << qMin(p.attempt, 10));
                p.deadline_ms = now + next;
                emit resend(p);
                // Collect updates instead of modifying during iteration
                toUpdate << it.key();
                updates[it.key()] = p;
            } else {
                toRemove << it.key();
                // Bounded dead-letter buffer (ring, size 128)
                if (dead_letters.size() >= 128) {
                    dead_letters.pop_front();
                }
                dead_letters.push_back(DeadLetter{p.msg_id, p.receiver_id, p.packet, now});
                emit failed(p.msg_id, p.receiver_id);
                emit deadLetter(p.msg_id, p.receiver_id, p.attempt, "max_retries_exceeded");
                appendDeadLetter(p.msg_id, p.receiver_id, p.attempt, "max_retries_exceeded");
            }
        }
    }

    // Apply updates after iteration is complete
    for (const auto& key : toUpdate) {
        pending[key] = updates[key];
    }

    for (const auto& k : toRemove) pending.remove(k);
}

void AckManager::appendDeadLetter(qint64 id, const QString& rx, int attempts, const QString& reason) {
    QString path = ConfigManager::ref().logging.deadletter_file;
    if (path.isEmpty()) path = QStringLiteral("logs/dds_deadletter.ndjson");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::Append|QIODevice::Text)) {
        const auto line = QString(R"({"ts":%1,"message_id":%2,"receiver":"%3","attempts":%4,"reason":"%5"})")
                          .arg(QDateTime::currentMSecsSinceEpoch())
                          .arg(id)
                          .arg(rx)
                          .arg(attempts)
                          .arg(reason);
        f.write(line.toUtf8());
        f.write("\n");
        f.close();
    }
}
