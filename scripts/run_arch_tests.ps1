# Run Architecture Tests (PowerShell)
# This script runs architecture-focused tests on an already-built project.
# Assumes 'build' directory exists and tests are built.

ctest --test-dir build -R "(test_discovery_rx|test_discovery_tx|test_pub2sub_reliable|test_discovery_cycle|test_qos_failure)" --output-on-failure -V