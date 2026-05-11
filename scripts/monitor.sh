#!/usr/bin/env bash
# Open serial monitor for esp32_four env using PlatformIO
set -euo pipefail
~/.platformio/penv/bin/platformio device monitor -e esp32_four
