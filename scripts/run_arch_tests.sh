#!/bin/bash
# Run Architecture Tests (Bash)
# This script runs architecture-focused tests on an already-built project.
# Assumes 'build' directory exists and tests are built.

ctest -R "(test_discovery_rx|test_discovery_tx|test_pub2sub_reliable|test_discovery_cycle|test_qos_failure)" -V