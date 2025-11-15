#!/usr/bin/env bash
# Upload the firmware to the board using PlatformIO via python3
set -euo pipefail
# Optionally pass additional args to platformio (e.g., -t upload -e <env>)
python3 -m platformio run -e esp32_four -t upload
