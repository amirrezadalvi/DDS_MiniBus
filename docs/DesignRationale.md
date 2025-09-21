# Design Rationale

This document explains key design decisions in DDS Mini-Bus, focusing on trade-offs, alternatives considered, and alignment with the current codebase.

## Reliability via UDP + AckManager

**Decision:** Use UDP as the primary transport with a custom AckManager for reliable QoS, rather than TCP.

**Rationale:**
- UDP allows efficient broadcast/multicast for discovery, avoiding TCP's connection overhead.
- AckManager provides fine-grained reliability control (ACK/retry with exponential backoff) without TCP's head-of-line blocking.
- Suitable for real-time pub/sub where some message loss is tolerable but guaranteed delivery is needed for critical data.

**Alternatives Considered:**
- Pure TCP: Simpler but lacks efficient multicast; connection-oriented nature complicates peer discovery.
- Reliable multicast protocols (e.g., PGM): Not widely supported in Qt/C++ standard libraries; adds complexity.

## JSON/CBOR Serialization with Negotiation

**Decision:** Support both JSON and CBOR with runtime format negotiation, preferring JSON.

**Rationale:**
- JSON is human-readable for debugging and interoperability.
- CBOR is compact for bandwidth-constrained environments.
- Negotiation ensures compatibility between peers with different preferences.
- Qt provides native support for both formats.

**Alternatives Considered:**
- Single format (JSON only): Limits efficiency; CBOR-only reduces readability.
- Protocol Buffers: Adds external dependency; overkill for this lightweight bus.

## Discovery Modes Including Loopback

**Decision:** Support broadcast, multicast, and loopback modes for discovery.

**Rationale:**
- Broadcast/multicast enable network-wide discovery.
- Loopback mode (`DDS_TEST_LOOPBACK=1`) allows local testing without network dependencies, crucial for Windows/MinGW where single-process tests are unstable.

**Alternatives Considered:**
- Centralized discovery service: Adds single point of failure; increases complexity.
- mDNS/Zeroconf: Platform-dependent; Qt lacks built-in support.

## Disabling Single-Process Integration Test on Windows

**Decision:** Disable `test_integration_scenarios` by default on Windows/MinGW due to Qt event-loop limitations.

**Rationale:**
- Qt's event loop can be unstable when multiple QCoreApplications run in the same process on Windows.
- E2E behavior is validated via multi-process demos (PowerShell scripts).
- Prevents flaky tests while ensuring functionality.

**Alternatives Considered:**
- Force-enable with environment variable: Allows opt-in for advanced users.
- Rewrite tests to avoid multiple event loops: Time-consuming; multi-process approach is more robust.

## Future Work

- Add TLS support for secure transports.
- Implement topic wildcards or hierarchies.
- Extend QoS policies (e.g., priority, durability).
- Add metrics/monitoring endpoints.
- Support for larger deployments with hierarchical discovery.
