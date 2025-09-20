## Architecture

### Component Diagram

```plantuml
@startuml DDS Mini-Bus Components
!theme plain
skinparam backgroundColor #FEFEFE
skinparam componentStyle uml2

package "DDS Logic Layer" {
  component [DDSCore] as DDSCore
  component [Publisher] as Publisher
  component [Subscriber] as Subscriber
  component [AckManager] as AckManager
  component [DiscoveryManager] as DiscoveryManager
  component [ConfigManager] as ConfigManager
  component [Topic Registry] as TopicRegistry
  component [QoS Manager] as QoSManager
}

package "Network Transport Layer" {
  component [ITransport] as ITransport
  component [UdpTransport] as UdpTransport
  component [TcpTransport] as TcpTransport
}

package "Serialization Layer" {
  component [Serializer] as Serializer
}

package "Utilities" {
  component [Logger] as Logger
}

' DDS Logic Layer relationships
DDSCore --> Publisher : creates
DDSCore --> Subscriber : creates
DDSCore --> TopicRegistry : manages
DDSCore --> AckManager : uses
DDSCore --> DiscoveryManager : uses
DDSCore --> QoSManager : uses
DDSCore --> ConfigManager : reads

Publisher --> Serializer : serializes
Subscriber --> Serializer : deserializes

AckManager --> QoSManager : implements
DiscoveryManager --> ConfigManager : reads

' Transport Layer relationships
DDSCore --> ITransport : sends/receives
ITransport <|-- UdpTransport : implements
ITransport <|-- TcpTransport : implements

UdpTransport --> Serializer : uses
TcpTransport --> Serializer : uses

' Cross-cutting
DDSCore --> Logger : logs
AckManager --> Logger : logs
DiscoveryManager --> Logger : logs
UdpTransport --> Logger : logs
TcpTransport --> Logger : logs

@enduml
```

### Sequence Diagram - Reliable QoS Message Flow

```plantuml
@startuml Reliable QoS Message Flow
!theme plain
skinparam backgroundColor #FEFEFE

actor Publisher
participant DDSCore
participant Serializer
participant UdpTransport
participant Network
participant UdpTransport as UdpTransportRX
participant Serializer as SerializerRX
participant Subscriber
participant AckManager

== Message Publication ==
Publisher -> DDSCore: publish(topic, payload, QoS=reliable)
DDSCore -> Serializer: encodeData(messageEnvelope)
Serializer --> DDSCore: encoded frame
DDSCore -> UdpTransport: send(frame, peer_ip, peer_port)
UdpTransport -> Network: UDP unicast packet

== Message Reception ==
Network -> UdpTransportRX: UDP packet received
UdpTransportRX -> SerializerRX: decode(frame)
SerializerRX --> UdpTransportRX: decoded messageEnvelope
UdpTransportRX -> DDSCore: onDatagram(bytes, from_ip, from_port)
DDSCore -> Subscriber: deliverToLocal(topic, payload)

== ACK Processing ==
Subscriber -> DDSCore: QoS=reliable detected
DDSCore -> Serializer: encodeAck(message_id, node_id)
Serializer --> DDSCore: encoded ACK frame
DDSCore -> UdpTransport: send(ack_frame, from_ip, from_port)
UdpTransport -> Network: UDP unicast ACK

== ACK Reception ==
Network -> UdpTransport: ACK packet received
UdpTransport -> Serializer: decode(ack_frame)
Serializer --> UdpTransport: decoded ACK
UdpTransport -> DDSCore: onDatagram(ack_bytes, from_ip, from_port)
DDSCore -> AckManager: ackReceived(message_id, receiver_id)
AckManager --> DDSCore: message acknowledged

== Timeout Handling ==
... timeout period ...
AckManager -> DDSCore: resendPacket(pending_message)
DDSCore -> UdpTransport: send(frame, peer_ip, peer_port)
note right: Retry logic with exponential backoff

@enduml
```

### Sequence Diagram - Discovery Flow

```plantuml
@startuml Discovery Flow
!theme plain
skinparam backgroundColor #FEFEFE

participant NodeA
participant DiscoveryManagerA
participant UdpTransportA
participant Network
participant UdpTransportB
participant DiscoveryManagerB
participant NodeB

== Discovery Announcement ==
NodeA -> DiscoveryManagerA: start()
DiscoveryManagerA -> DiscoveryManagerA: getSupportedSerialization()
DiscoveryManagerA -> Serializer: encodeDiscovery(nodeId, topics, proto, ts, port, serialization)
Serializer --> DiscoveryManagerA: discovery_frame
DiscoveryManagerA -> UdpTransportA: send(discovery_frame, broadcast/multicast)
UdpTransportA -> Network: UDP broadcast packet

== Discovery Reception ==
Network -> UdpTransportB: discovery packet received
UdpTransportB -> Serializer: decode(discovery_frame)
Serializer --> UdpTransportB: decoded discovery data
UdpTransportB -> DiscoveryManagerB: processPendingDatagrams()
DiscoveryManagerB -> DiscoveryManagerB: validate discovery packet
DiscoveryManagerB --> NodeB: peerUpdated(peerId, discovery_payload)

== Peer Registration ==
NodeB -> DDSCore: updatePeers(peerId, payload)
DDSCore -> DDSCore: store peer info + serialization prefs
note right: DDSCore now knows peer's supported serialization formats

@enduml
```

## Message & Frame Model

- MessageEnvelope: { message_id, topic, qos, timestamp, from_node, payload }.
- Frames: DATA, ACK, DISCOVERY.
- Serialization: JSON (default) or CBOR with per-peer negotiation.

## QoS & Routing

- Best-effort: broadcast to transport.udp.port.
- Reliable: UNICAST to all peers advertising the topic with ACK/retry.

## De-duplication

- Subscriber/DDSCore: bounded LRU for (publisher, topic, message_id), capacity 2048 (configurable via qos.dedup_capacity), evicts oldest on overflow.

## Serialization Negotiation

- Discovery beacons include `serialization` field with supported formats array
- Per-peer format selection: first intersection between local and peer supported lists
- Fallback to JSON if no common format
- Auto-detection on receive: CBOR first, then JSON

## Logging & Categories

- [BOOT], [DISC], [SEND][UNICAST], [SEND][BCAST], [ACK][TX/RX], [RETRY], [GIVEUP], [DEADLETTER], [DROP], [ROUTE][MISS].

## Testing

Unit tests for Serializer, AckManager, and Negotiation verify core functionality:
- Serializer: Validates JSON/CBOR decode for DATA, ACK, DISCOVERY; drops malformed/missing fields.
- AckManager: Tests retry logic (resend on timeout, dead-letter after max retries).
- Negotiation: Tests format selection algorithms and edge cases.

Run via `scripts/test_all.ps1` or CTest.
