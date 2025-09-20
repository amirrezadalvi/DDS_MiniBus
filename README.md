# DDS Mini-Bus

Lightweight DDS-like bus with publish/subscribe, discovery, QoS (best_effort, reliable).

## Build

Requires Qt6.5+, CMake 3.16+.

```bash
mkdir build && cd build
cmake .. -G Ninja
ninja
```

On Windows, use MSVC or MinGW.

## Run

Edit config.json.

Run dds_mini_bus.exe

## How to Run

### Quick Demo

1. **Terminal 1 (Receiver):**
   ```bash
   ./scripts/run_rx.sh
   ```
   Or on Windows:
   ```batch
   scripts\run_rx.bat
   ```

2. **Terminal 2 (Sender):**
   ```bash
   ./scripts/run_tx.sh
   ```
   Or on Windows:
   ```batch
   scripts\run_tx.bat
   ```

### Expected Output

**Receiver (Terminal 1):**
```
cli: role=subscriber topic=sensor/temperature qos=reliable cfg=config/config_rx.json format=json
discovery: announce node=dds-receiver-1 topics=2 port=39011 formats=("json", "cbor")
[ts=2025-01-16 15:30:45.123] topic=sensor/temperature qos=reliable msg_id=12345 payload={"value":23.5,"unit":"C"}
```

**Sender (Terminal 2):**
```
cli: role=sender topic=sensor/temperature qos=reliable cfg=config/config_tx.json format=json
discovery: announce node=dds-sender-1 topics=2 port=39010 formats=("json", "cbor")
discovery: peer=dds-receiver-1 topics=2 (ver=1.0, formats=("json", "cbor"))
negotiate: chosen=json local=("json", "cbor") remote=("json", "cbor")
cli: sent msg_id=12345 topic=sensor/temperature qos=reliable
qos: [ACK][RX] 12345 from dds-receiver-1
```

### Custom Configuration

Override settings via environment variables:

```bash
# Custom topic and QoS
TOPIC=my/sensor QOS=best_effort ./scripts/run_rx.sh

# Custom payload and count
PAYLOAD='{"temperature": 25.0}' COUNT=10 ./scripts/run_tx.sh
```

### CLI Options

The main app (dds_mini_bus.exe) supports the following CLI options as per the PDF spec:

```
DDS Mini-Bus CLI

Usage: dds_mini_bus [options]

Options:
  --role <sender|subscriber>    Required: Run as sender or subscriber
  --topic <string>              Topic to publish/subscribe (default: sensor/temperature)
  --qos <reliable|best_effort>  QoS level (default: reliable)
  --count <int>                 Number of messages to send (default: 1, sender only)
  --interval-ms <int>           Interval between messages in ms (default: 1000, sender only)
  --payload <json>              JSON payload for messages (default: {"value":23.5,"unit":"C"}, sender only)
  --config <path>               Config file path (default: config/config.json)
  --log-level <debug|info|warn|error>  Log level (default: info)
  --help, -h                    Show this help message

Examples:
  ./dds_mini_bus --role sender --topic sensor/temp --qos reliable --count 5
  ./dds_mini_bus --role subscriber --topic sensor/temp --log-level debug
```

Example mirroring PDF:
```
.\build\dds_mini_bus.exe --role sender --topic sensor/temperature --qos reliable --count 3 --config .\config\config_tx.json
```

Note: Test binaries (test_discovery_*.exe) read settings from config files only and do not support CLI flags like --count/--topic. Use the main app for interactive testing with CLI options.

Network tests in CTest are excluded by default to avoid hangs; run scenarios manually as shown in the test scenarios section.

### Test Scenarios (Reproducible Commands)

#### Unit (non-network) tests
```
ctest --test-dir build -E "test_discovery_rx|test_discovery_tx|test_pub2sub_reliable|test_discovery_cycle|test_qos_failure|test_tcp_reliable" -Q --timeout 60 --output-log evidence\ctest\ctest_units_only.txt
```

#### Perf
```
ctest --test-dir build -R test_throughput_udp   -Q --timeout 90 --output-log evidence\ctest\throughput_udp.txt
ctest --test-dir build -R test_latency_reliable -Q --timeout 90 --output-log evidence\ctest\latency_reliable.txt
```

#### Pub → 2 Subs (two RX + one TX)
```
start "RX1" cmd /c ".\build\test_discovery_rx.exe --config .\config\config_rx.json  > evidence\logs\rx1.log"
start "RX2" cmd /c ".\build\test_discovery_rx.exe --config .\config\config_rx2.json > evidence\logs\rx2.log"
timeout /t 2 >nul
.\build\test_discovery_tx.exe --config .\config\config_tx.json >> evidence\logs\tx.log
.\build\test_discovery_tx.exe --config .\config\config_tx.json >> evidence\logs\tx.log
.\build\test_discovery_tx.exe --config .\config\config_tx.json >> evidence\logs\tx.log
```

#### QoS Failure
```
start "RX1" cmd /c ".\build\test_discovery_rx.exe --config .\config\config_rx.json > evidence\logs\qosfail_rx1.log"
.\build\test_discovery_tx.exe --config .\config\config_tx.json > evidence\logs\qosfail_tx.log
taskkill /IM test_discovery_rx.exe /F 2>nul
```

#### Discovery-only (5s window)
```
start "DiscTX" cmd /c ".\build\test_discovery_tx.exe --config .\config\config_tx.json > evidence\logs\disc_tx.log"
start "DiscRX" cmd /c ".\build\test_discovery_rx.exe --config .\config\config_rx.json    > evidence\logs\disc_rx1.log"
timeout /t 5 >nul
taskkill /IM test_discovery_tx.exe /F 2>nul
taskkill /IM test_discovery_rx.exe /F 2>nul
```

## Windows Qt Setup & Deployment

For Windows development with Qt 6.5+ (MSVC or MinGW):

1. Install Qt 6.5+ with MSVC 2019 x64 or MinGW from Qt Installer.
2. Set environment variables:
   ```cmd
   set QT_BIN=C:\Qt\6.5.3\msvc2019_64\bin
   set PATH=%QT_BIN%;%PATH%
   ```
3. Build and deploy:
   ```cmd
   mkdir build && cd build
   cmake -S .. -B . -G "Visual Studio 16 2019" -A x64
   cmake --build . --config Release
   ```
4. Deploy Qt runtime (makes EXE portable):
   ```cmd
   %QT_BIN%\windeployqt.exe dds_mini_bus.exe --debug --compiler-runtime --network
   ```
5. Smoke test:
   ```cmd
   dds_mini_bus.exe --help
   ```
   Should show CLI options without Qt DLL errors.

### DLL Sanity Check

Before running, verify Qt DLLs resolve correctly:

```cmd
where Qt6Core.dll
where Qt6Network.dll
where libstdc++-6.dll
```

All should point to Qt/MinGW paths, not system or other installations.

### Running Triple Test

Use the provided script to launch 1 sender + 2 subscribers:

```cmd
cd build
run_triple.bat
```

This opens three console windows with correct PATH and commands. All should show immediate output; no silent terminals.

Working directory: `build/`

Config files: Copied automatically to `build/config/` on build.

Logs: Written to `build/logs/` directory.

## Windows Quickstart

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_win.ps1
.\scripts\run_rx.ps1
.\scripts\run_tx.ps1
```

First send occurs after peer discovery; you should see only [SEND][UNICAST] as the first send.

For TCP transport:

```powershell
.\scripts\run_tcp_rx.ps1
.\scripts\run_tcp_tx.ps1
```

Note reliable→UNICAST, best_effort→BCAST behavior; discovery multicast 239.255.0.1:45454.

For tests:

run_tx.ps1

run_rx.ps1

## Configuration Reference

DDS Mini-Bus uses JSON configuration files. All options are documented below with their default values.

### Root Configuration

```json
{
  "node_id": "node-1",
  "protocol_version": "1.0",
  "topics": ["sensor/temperature", "sensor/humidity"]
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `node_id` | string | `"node-1"` | Unique identifier for this node in the network |
| `protocol_version` | string | `"1.0"` | Protocol version for compatibility checking |
| `topics` | array | `[]` | List of topics this node is interested in |

### Discovery Configuration

```json
{
  "discovery": {
    "enabled": true,
    "mode": "multicast",
    "address": "239.255.0.1",
    "port": 45454,
    "interval_ms": 1000
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | boolean | `true` | Enable/disable discovery mechanism |
| `mode` | string | `"broadcast"` | `"broadcast"` or `"multicast"` |
| `address` | string | `"239.255.0.1"` | Multicast group address (ignored in broadcast mode) |
| `port` | number | `45454` | UDP port for discovery traffic |
| `interval_ms` | number | `1000` | Discovery beacon interval in milliseconds |

### Transport Configuration

```json
{
  "transport": {
    "default": "udp",
    "udp": {
      "port": 38020,
      "rcvbuf": 262144,
      "sndbuf": 262144
    },
    "tcp": {
      "listen": true,
      "port": 38030,
      "connect": ["127.0.0.1:38030"],
      "rcvbuf": 262144,
      "sndbuf": 262144,
      "connect_timeout_ms": 1500,
      "heartbeat_ms": 0,
      "reconnect_backoff_ms": 500,
      "max_reconnect_attempts": 10
    }
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `default` | string | `"udp"` | Default transport protocol (`"udp"` or `"tcp"`) |

#### UDP Transport Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `port` | number | `38020` | UDP port for data traffic |
| `rcvbuf` | number | `262144` | Receive buffer size in bytes |
| `sndbuf` | number | `262144` | Send buffer size in bytes |

#### TCP Transport Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `listen` | boolean | `true` | Accept incoming TCP connections |
| `port` | number | `38030` | TCP port for listening |
| `connect` | array | `[]` | List of `host:port` strings for outgoing connections |
| `rcvbuf` | number | `262144` | Receive buffer size in bytes |
| `sndbuf` | number | `262144` | Send buffer size in bytes |
| `connect_timeout_ms` | number | `1500` | Connection timeout in milliseconds |
| `heartbeat_ms` | number | `0` | Heartbeat interval (0 = disabled) |
| `reconnect_backoff_ms` | number | `500` | Reconnection backoff delay |
| `max_reconnect_attempts` | number | `10` | Maximum reconnection attempts |

### QoS Configuration

```json
{
  "qos": {
    "default": "best_effort",
    "reliable": {
      "ack_timeout_ms": 200,
      "max_retries": 3,
      "exponential_backoff": true
    },
    "dedup_capacity": 2048
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `default` | string | `"best_effort"` | Default QoS level (`"best_effort"` or `"reliable"`) |
| `dedup_capacity` | number | `2048` | LRU cache size for message de-duplication |

#### Reliable QoS Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ack_timeout_ms` | number | `200` | ACK timeout in milliseconds |
| `max_retries` | number | `3` | Maximum retry attempts |
| `exponential_backoff` | boolean | `true` | Use exponential backoff for retries |

### Serialization Configuration

```json
{
  "serialization": {
    "format": "json",
    "supported": ["json", "cbor"]
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `format` | string | `"json"` | Preferred serialization format (`"json"` or `"cbor"`) |
| `supported` | array | `["json", "cbor"]` | Supported formats in preference order |

### Logging Configuration

```json
{
  "logging": {
    "level": "info",
    "file": "logs/dds.log"
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `level` | string | `"info"` | Log level (`"debug"`, `"info"`, `"warning"`, `"error"`) |
| `file` | string | `""` | Log file path (empty = console only; relative to working directory) |

## Configs

config.json: main config.

config_rx.json, config_tx.json: for tests.

**Note:** Live config reload is not implemented. Restart the application to apply configuration changes.

## Binary (CBOR) Serialization & Negotiation

DDS Mini-Bus supports both JSON and CBOR (Concise Binary Object Representation) serialization formats for efficient network communication.

### Configuration

```json
{
  "serialization": {
    "format": "cbor",
    "supported": ["cbor", "json"]
  }
}
```

- `format`: Your preferred format ("json" | "cbor"), default "json"
- `supported`: Array of supported formats in preference order, default ["format", "other"]

### Per-Peer Negotiation

- Discovery beacons include a `serialization` field with supported formats
- When sending DATA/ACK packets, the system negotiates the best common format:
  - First intersection between your `supported` list and peer's `supported` list
  - Falls back to JSON if no common format
- Receive path auto-detects format (CBOR first, then JSON fallback)

### Examples

**CBOR preferred:**
```json
{"serialization": {"format": "cbor", "supported": ["cbor", "json"]}}
```

**JSON-only:**
```json
{"serialization": {"format": "json", "supported": ["json"]}}
```

**Mixed compatibility:**
```json
{"serialization": {"format": "cbor", "supported": ["cbor", "json"]}}
```

### Usage

Run with custom config:
```powershell
.\build\dds_mini_bus.exe --config .\config.json
```

Test discovery with serialization:
```powershell
.\build\test_discovery_tx.exe --config .\config.json --qos reliable --interval-ms 800
```

### Benefits

- **CBOR**: More compact binary format, faster parsing, lower bandwidth
- **JSON**: Human-readable, easier debugging, maximum compatibility
- **Negotiation**: Automatic format selection per peer
- **Fallback**: Graceful degradation when peers don't support preferred format
- **Auto-detection**: No configuration needed for receiving mixed-format traffic

## Logs

To logs/dds.log

## Troubleshooting

- Firewall: allow UDP/TCP ports.
- Missing DLLs: ensure Qt/MinGW in PATH.
- Address reuse: Windows UDP broadcast nuances.

## Run tests

**Test Status:** All unit and loopback integration tests pass. Network/multicast tests are skipped unless explicitly enabled (require firewall exceptions).

### Unit Tests (Always Pass)
```bash
ctest -L unit -V  # 6/6 tests pass
```

### Loopback Integration Tests (Hermetic)
```bash
set DDS_TEST_LOOPBACK=1
ctest -L integration_loopback -V  # Tests pass without network
```

### Network/Multicast Tests (Optional)
```bash
# Enable multicast tests (requires firewall exceptions for UDP ports)
set ALLOW_MULTICAST_TESTS=1
ctest -L network -V  # May pass with proper network setup
```

### Manual Test Commands
- Unit tests: `ctest -R "test_serializer|test_ack_manager|test_negotiation|test_tcp_reliable|test_throughput_udp|test_latency_reliable" -V`
- Loopback mode: `set DDS_TEST_LOOPBACK=1 && ctest -R "test_pub2sub_reliable|test_discovery_cycle|test_qos_failure" -V`

**Note:** Test binaries (test_discovery_*.exe) read settings from config files only. The main app (dds_mini_bus.exe) supports CLI flags like --count/--topic/--qos.

## Known issues

Windows broadcast requires binding to same port.

TCP reconnect stops after `max_reconnect_attempts` (default 10) with `[TCP][GIVEUP]`; increase in config if needed.

Bounded LRU de-dup (capacity from `qos.dedup_capacity`, default 2048).

Safe decode drops malformed frames with `[DROP][DECODE]`.

## Performance Tests

Run throughput and latency tests manually:

```bash
# Throughput (UDP, best_effort)
./build/test_throughput_udp

# Latency (reliable)
./build/test_latency_reliable
```

These tests measure msgs/sec and latency stats. Adjust config for different rates/sizes.