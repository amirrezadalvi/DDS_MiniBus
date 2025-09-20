#pragma once

#include <cstdint>
#include <string>
#include <QString>

namespace mbus {

// QoS levels
enum class QosLevel { BestEffort, Reliable };

// Reliable settings (config-driven)
struct ReliableSettings {
  uint32_t ack_timeout_ms{300};
  uint32_t max_retries{3};
  uint32_t backoff_initial_ms{200};
  double   backoff_factor{2.0};
};

// Effective QoS policy for a publication
struct QosPolicy {
  QosLevel          level{QosLevel::BestEffort};
  ReliableSettings  reliable{};
  // If enabled, remember the most recent message per topic and deliver it
  // to a newly-added *local* subscriber on that topic.
  bool retain_last{false};
};

} // namespace mbus

inline bool isReliable(const QString& qos) {
    return qos.compare("reliable", Qt::CaseInsensitive) == 0;
}
