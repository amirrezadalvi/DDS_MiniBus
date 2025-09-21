#include "subscriber.h"
#include "dds_core.h"
Subscriber::Subscriber(DDSCore& c, const QString& t, Callback cb)
    : core(c), topic(t), callback(std::move(cb)) {}