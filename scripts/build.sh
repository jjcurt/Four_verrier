#!/usr/bin/env bash
# Build the esp32_four environment using PlatformIO via python3
set -euo pipefail
python3 -m platformio run -e esp32_four
