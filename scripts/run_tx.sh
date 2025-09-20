#!/usr/bin/env bash
set -euo pipefail

# DDS Mini-Bus Sender Demo Script
# Usage: ./run_tx.sh [TOPIC] [QOS] [COUNT] [INTERVAL] [PAYLOAD]

BIN="${BIN:-./build/dds_mini_bus}"
CFG="${CFG:-../config/config_tx.json}"
TOPIC="${TOPIC:-sensor/temperature}"
QOS="${QOS:-reliable}"
COUNT="${COUNT:-5}"
INTERVAL="${INTERVAL:-500}"
PAYLOAD="${PAYLOAD:-'{"value": 23.5, "unit": "C"}'}"

if [ ! -f "$BIN" ]; then
    echo "Error: Binary not found at $BIN"
    echo "Please build the project first: mkdir build && cd build && cmake .. && make"
    exit 1
fi

if [ ! -f "$CFG" ]; then
    echo "Error: Config not found at $CFG"
    exit 1
fi

echo "Starting DDS sender..."
echo "  Binary: $BIN"
echo "  Config: $CFG"
echo "  Topic: $TOPIC"
echo "  QoS: $QOS"
echo "  Count: $COUNT"
echo "  Interval: ${INTERVAL}ms"
echo "  Payload: $PAYLOAD"
echo ""

exec "$BIN" --role sender --topic "$TOPIC" --qos "$QOS" --count "$COUNT" --interval-ms "$INTERVAL" --payload "$PAYLOAD" --config "$CFG" --log-level debug