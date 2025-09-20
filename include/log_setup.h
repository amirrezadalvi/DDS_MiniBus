#pragma once
#include <QString>
namespace LogSetup {
    void init(const QString& level, const QString& filePath);
    void setLevel(const QString& level);
}
