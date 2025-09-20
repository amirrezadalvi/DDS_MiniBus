#pragma once
#include <QString>
#include <QJsonObject>
class DDSCore;
class Publisher {
public:
    Publisher(DDSCore& core, const QString& topic);
    qint64 publish(const QJsonObject& payload, const QString& qos = "best_effort");
private:
    DDSCore& core; QString topic;
};
