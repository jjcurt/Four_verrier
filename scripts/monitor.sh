#!/usr/bin/env bash
# Open serial monitor for esp32_four env using PlatformIO via python3
set -euo pipefail
python3 -m platformio device monitor -e esp32_four
