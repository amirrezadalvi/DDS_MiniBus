#include "publisher.h"
#include "dds_core.h"
#include "qos.h"
#include <QDateTime>

Publisher::Publisher(DDSCore& c, const QString& t) : core(c), topic(t) {}
qint64 Publisher::publish(const QJsonObject& payload, const QString& qos) {
    return core.publishInternal(topic, payload, qos);
}
