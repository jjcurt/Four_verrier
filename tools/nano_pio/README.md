# Nano Relay Controller (PlatformIO build)

This directory contains a PlatformIO project for building and uploading the Nano relay controller sketch.

## Overview

The main sketch is kept in `tools/nano/relay_nano.ino` to avoid duplication. This project provides:
1. A proper PlatformIO build environment
2. Upload scripts for flashing the Nano
3. Symlink-based source management

## File Structure

- `platformio.ini` - Project configuration for Nano
- `src/relay_nano.ino` - Symlink to `../nano/relay_nano.ino`
- `upload_nano.py` - Python upload script with port auto-detection
- `upload_nano.sh` - Shell wrapper script (uses Python script)

## Building

```bash
# Build only
pio run -e nano

# Build and upload (auto-detect port)
python3 upload_nano.py

# Build and upload (explicit port)
python3 upload_nano.py --port /dev/ttyUSB0
```

## Notes

- The Python upload script prefers `pio` command but falls back to `platformio`
- Serial ports are auto-detected from common names like `/dev/ttyUSB0`
- Default baud is 115200 (configured in `platformio.ini`)

## Upload Script Help

```bash
python3 upload_nano.py --help
```

Options:
- `--port PORT` - Specify serial port
- `--dry-run` - Show pio command without running
- `--verbose` - Show extra debug info
