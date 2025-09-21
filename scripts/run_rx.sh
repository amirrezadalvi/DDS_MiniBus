#!/usr/bin/env bash
set -euo pipefail

# DDS Mini-Bus Receiver Demo Script
# Usage: ./run_rx.sh [TOPIC] [CONFIG]

BIN="${BIN:-./build/dds_mini_bus}"
CFG="${CFG:-../config/config_rx.json}"
ROLE="${ROLE:-subscriber}"
TOPIC="${TOPIC:-sensor/temperature}"

if [ ! -f "$BIN" ]; then
    echo "Error: Binary not found at $BIN"
    echo "Please build the project first: mkdir build && cd build && cmake .. && make"
    exit 1
fi

if [ ! -f "$CFG" ]; then
    echo "Error: Config not found at $CFG"
    exit 1
fi

echo "Starting DDS receiver..."
echo "  Binary: $BIN"
echo "  Config: $CFG"
echo "  Role: $ROLE"
echo "  Topic: $TOPIC"
echo ""

exec "$BIN" --role "$ROLE" --topic "$TOPIC" --config "$CFG" --log-level info