#!/usr/bin/env python3
"""Upload helper for the Nano sketch using PlatformIO.

Usage examples:
  # Auto-detect a likely port and upload
  python3 tools/nano_pio/upload_nano.py

  # Specify a port explicitly
  python3 tools/nano_pio/upload_nano.py --port /dev/ttyUSB0

  # Dry-run (show the pio command without running)
  python3 tools/nano_pio/upload_nano.py --dry-run

This script prefers `pio` on PATH but will try `platformio` as a fallback.
It runs PlatformIO in the `tools/nano_pio` subproject to upload the `nano` env.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


DEFAULT_PORT_CANDIDATES = ["/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyACM0", "/dev/ttyACM1"]
PROJECT_DIR = Path(__file__).resolve().parents[1]
NANO_SUBDIR = PROJECT_DIR / "tools" / "nano_pio"


def find_pio_executable() -> list[str]:
    """Return a command prefix to invoke PlatformIO.

    Prefer a direct 'pio' or 'platformio' binary if available. If not,
    fall back to using the current Python interpreter to run PlatformIO as
    a module (e.g. '/usr/bin/python3 -m platformio'). The return value is a
    list of command parts suitable for prefixing the PlatformIO args.
    """
    for name in ("pio", "platformio"):
        path = shutil.which(name)
        if path:
            return [path]
    # Fallback: use current Python interpreter to run platformio as a module
    return [sys.executable, "-m", "platformio"]


def auto_detect_port() -> str | None:
    for p in DEFAULT_PORT_CANDIDATES:
        if Path(p).exists():
            return p
    return None


def build_pio_command(pio_prefix: list[str], port: str) -> list[str]:
    """Build a command list for subprocess.run from a pio prefix list."""
    return pio_prefix + [
        "run",
        "-d",
        str(NANO_SUBDIR),
        "-e",
        "nano",
        "-t",
        "upload",
        "--upload-port",
        port,
    ]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Upload Nano sketch via PlatformIO subproject (tools/nano_pio)")
    parser.add_argument("--port", "-p", help="Serial port to upload to (e.g. /dev/ttyUSB0)")
    parser.add_argument("--dry-run", action="store_true", help="Print the pio command but don't run it")
    parser.add_argument("--verbose", "-v", action="store_true", help="Print extra debug info")

    args = parser.parse_args(args=argv)

    pio_prefix = find_pio_executable()
    if not pio_prefix:
        print("Error: PlatformIO CLI not found. Please install PlatformIO or make sure it's importable as a Python module.")
        return 2

    port = args.port
    if not port:
        port = auto_detect_port()
        if args.verbose:
            print(f"Auto-detected port: {port}")
    if not port:
        print("No serial port specified and none of the common candidates found.")
        print("Please specify a port with --port /dev/ttyUSB0")
        return 3

    cmd = build_pio_command(pio_prefix, port)

    print("PlatformIO command:")
    print(" ".join(cmd))

    if args.dry_run:
        print("Dry-run: not executing")
        return 0

    try:
        subprocess.run(cmd, check=True)
        print("Upload finished successfully.")
        return 0
    except subprocess.CalledProcessError as e:
        print(f"PlatformIO upload failed with exit code {e.returncode}")
        return e.returncode


if __name__ == "__main__":
    raise SystemExit(main())
