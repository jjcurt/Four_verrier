#!/usr/bin/env bash
# Upload the firmware to the board using PlatformIO
set -euo pipefail
~/.platformio/penv/bin/platformio run -e esp32_four -t upload
