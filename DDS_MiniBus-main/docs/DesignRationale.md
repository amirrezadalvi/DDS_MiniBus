## Design Rationale

- Lightweight: minimal dependencies, Qt6.

- Cross-platform: Windows, Linux.

- Config-driven: JSON config.

- Tradeoffs: JSON serialization for simplicity, not binary.

- Discovery: multicast preferred, broadcast fallback.

- QoS: reliable with ACK/retry, best-effort broadcast.
