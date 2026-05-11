#!/usr/bin/env bash
# Build the esp32_four environment using PlatformIO
set -euo pipefail
~/.platformio/penv/bin/platformio run -e esp32_four
