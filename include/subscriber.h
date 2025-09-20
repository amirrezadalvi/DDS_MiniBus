// core/subscriber.h
#pragma once
#include <QString>
#include <QJsonObject>
#include <functional>

class DDSCore;  // forward decl

class Subscriber {
public:
    using Callback = std::function<void(const QJsonObject&)>;
    Subscriber(DDSCore& core, const QString& topic, Callback cb);
    QString topicName() const { return topic; }
private:
    DDSCore& core;
    QString  topic;
    Callback callback;
    friend class DDSCore;
};
