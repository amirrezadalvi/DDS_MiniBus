# DDS Mini-Bus Architecture

This document describes the high-level architecture and runtime behavior of the DDS Mini-Bus implementation. For brevity, diagrams are provided in PlantUML under `docs/diagrams/`.

## Goals
- Pub/Sub messaging with topic-based routing
- Lightweight Discovery with **Broadcast/Multicast** modes and **Loopback** mode for local demos
- Two transports: **UDP** and **TCP**
- Two serialization formats: **JSON** and **CBOR** with runtime negotiation
- QoS: **Best-Effort** and **Reliable** (ACK + limited retry with **exponential backoff**)
- Simple JSON configuration with sensible defaults and **runtime partial parameter updates**

## Architecture Overview
See the component diagram: `docs/diagrams/component.puml`.

**Key Modules:**

- **DDSCore**
Orchestrates node lifecycle. Holds Publisher/Subscriber registries, owns `DiscoveryManager`, a `TransportBase` instance (default UDP), and a `Serializer`. Negotiates format with each Peer and routes messages to eligible peers.

- **Publisher / Subscriber**
*Publisher* has `(topic, qos, formatPreference)` and delegates payload to Core. *Subscriber* subscribes to a Topic and receives decoded objects; caches last message if needed.

- **DiscoveryManager**
Announces/learns peer presence and capabilities (Topics, data ports, supported formats, protocol version). Modes:
- **Multicast** (e.g., 239.255.0.1)
- **Broadcast** (255.255.255.255)
- **Loopback** (with `DDS_TEST_LOOPBACK=1` for local tests)
Emits events like `peerUpdated` that Core uses for routing updates.

- **TransportBase / UdpTransport / TcpTransport**
Common layer for sending/receiving packets (Envelope). `UdpTransport` is the default data plane (Unicast to Peer's data port); `TcpTransport` used in related tests. Reliable QoS uses `AckManager` to track in-flight messages, timeouts, and retries.

- **AckManager**
Maps `message_id` to delivery status for reliable QoS. Implements limited retry with exponential backoff and logs warnings/Dead-Letter after exhausting attempts.

- **Serializer**
Supports **JSON** and **CBOR**. Core negotiates common format when establishing links (prefers JSON).

- **ConfigManager**
Loads `config.json` including Discovery modes/ports, data ports, QoS settings, and Logging. Some parameters reloadable without restart.

- **Logger (Qt Categories)**
Categories like `dds.net` and `dds.disc`. For demos/tests, enable verbose logs with `QT_LOGGING_RULES="dds.disc=true;dds.net=true"`.

## Data Model (Envelope)
Minimal fields on wire:
- `topic` (string)
- `message_id` (unsigned int, ascending per node)
- `qos` ("best_effort" or "reliable")
- `format` ("json" or "cbor")
- `payload` (bytes) — output of chosen Serializer

**ACK** includes the original `message_id` and is sent by receiver when `qos == reliable`.

## Discovery Messages
Periodic announcements include:
- `node_id` (string)
- `version` (string)
- `topics` (list)
- `formats` (list, e.g., ["json","cbor"])
- `dataPort` (number)

## QoS (Reliable)
- Assign `message_id` and send to all routed peers
- Start tracking in `AckManager` with initial timeout `T` and backoff factor `k` (e.g., 2x)
- On timeout, retry up to limit `N`
- On ACK receipt, mark successful delivery and stop retries
- After `N` failures, log Dead-Letter/warning

See sequence diagram `docs/diagrams/sequence_publish_reliable.puml` for this flow.

## Threading and Event Loop
Qt event loop drives timers (Retries, Discovery beacons) and socket I/O. On Windows/MinGW, the single-process integration test that creates multiple Cores in one process may be unstable; thus it's **disabled by default** and E2E coverage done via multi-process demos (PowerShell).

## Configuration
`config/config.json` is copied to `build/qt_deploy/config/config.json` with build artifacts. Key fields:
- `discovery.mode`: one of `multicast` | `broadcast` | `loopback`
- `discovery.group`: IPv4 group or 255.255.255.255 (for Broadcast)
- `discovery.port`: control port (e.g., 39001)

How to view PlantUML diagrams: Install the PlantUML extension in VS Code to preview .puml files.