#pragma once
#include <QList>
#include <QSet>
#include <QString>

class BoundedLRU {
public:
    explicit BoundedLRU(int capacity) : cap(capacity) {}
    bool contains(const QString& key) const { return set.contains(key); }
    void insert(const QString& key) {
        if (set.contains(key)) return; // already seen, no update
        if (order.size() >= cap) {
            QString oldest = order.takeFirst();
            set.remove(oldest);
        }
        order.append(key);
        set.insert(key);
    }
private:
    int cap;
    QList<QString> order; // front is oldest
    QSet<QString> set;
};