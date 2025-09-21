# Architecture Tests

This directory contains documentation for architecture-focused tests. These tests validate key behaviors of the DDS Mini-Bus system without modifying the test code.

## Key Tests

The following existing tests represent core architecture behaviors:

- `test_discovery_rx`: Validates discovery message reception and peer learning.
- `test_discovery_tx`: Validates discovery announcement sending.
- `test_pub2sub_reliable`: Validates reliable publish/subscribe with ACK handling.
- `test_discovery_cycle`: Validates full discovery announce/learn/route cycle.
- `test_qos_failure`: Validates QoS failure handling (e.g., retries, dead letters).

## Running the Tests

Use the following commands to run these tests selectively.

**PowerShell (Windows):**
```powershell
ctest --test-dir build -R "(test_discovery_rx|test_discovery_tx|test_pub2sub_reliable|test_discovery_cycle|test_qos_failure)" --output-on-failure -V
```

**Bash (Linux/macOS):**
```bash
ctest -R "(test_discovery_rx|test_discovery_tx|test_pub2sub_reliable|test_discovery_cycle|test_qos_failure)" -V
```

These commands filter CTest to run only the architecture-relevant tests, providing verbose output for validation.