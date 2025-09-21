## Architecture Overview

### Components
- **DDS Core** – orchestrates publishers/subscribers, routing, QoS.
- **Publisher / Subscriber** – application-facing API for send/receive.
- **Serializer** – JSON/CBOR encode/decode with format negotiation.
- **Discovery Manager** – periodic announce/listen (broadcast/multicast or loopback).
- **Ack Manager** – reliable QoS (ack + bounded retries).
- **Transports** – UDP (unicast/broadcast/multicast), TCP.
- **Config Manager** – loads runtime options from JSON config.
- **Logger** – structured logging for net/discovery/test.

### Reliable Publish – Sequence
See `docs/diagrams/sequence_publish_reliable.puml`:

1. App → Publisher → Core
2. Core → Serializer (JSON/CBOR), returns bytes
3. Core selects peers from Discovery
4. UDP unicast send; receiver decodes and delivers
5. ACK returns; retries are bounded if necessary
