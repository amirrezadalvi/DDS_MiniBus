// core/topic.h
#pragma once
#include <QString>
#include <QStringList>
struct TopicInfo {
    QString    name;
    QString    dataType = "json";
    QString    qos_policy = "best_effort";
    qint64     lastMessageId = 0;
    QStringList subscribers;
};
