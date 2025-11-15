#!/usr/bin/env python3
"""
Serial test runner for Nano relay firmware.
Runs configurable test sequences and validates responses.
"""
import os
import time
import termios
import sys
import select
import argparse
from datetime import datetime


def setup_serial(port, baud=115200):
    """Configure serial port with given parameters."""
    try:
        fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(fd)
        attrs[4] = attrs[5] = termios.B115200
        attrs[2] = attrs[2] | termios.CREAD | termios.CLOCAL
        attrs[3] = attrs[3] & ~(termios.ECHO | termios.ICANON | termios.ISIG)
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        return fd
    except Exception as e:
        print(f"Error opening {port}: {e}", file=sys.stderr)
        sys.exit(1)


def log_message(msg, timestamp=True):
    """Log a message with optional timestamp."""
    if timestamp:
        now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{now}] {msg}")
    else:
        print(msg)
    sys.stdout.flush()


def send(fd, cmd, delay=0.12, timeout=0.4):
    """Send a command and read response with timeout."""
    log_message(f"SEND: {cmd}")
    try:
        os.write(fd, (cmd + "\n").encode())
    except Exception as e:
        log_message(f"Error sending {cmd}: {e}")
        return False

    time.sleep(delay)
    end = time.time() + timeout
    response_received = False

    while time.time() < end:
        try:
            r, w, x = select.select([fd], [], [], 0.1)
            if r:
                try:
                    data = os.read(fd, 4096)
                    if data:
                        log_message(f"RECV: {data.decode('ascii', errors='replace').strip()}")
                        response_received = True
                except BlockingIOError:
                    pass
        except select.error as e:
            log_message(f"Select error: {e}")
            break

    return response_received


def run_test_sequence(fd, tests=None):
    """Run a sequence of test commands."""
    if tests is None:
        # Default test sequence if none provided
        tests = [
            ('D1', 'Enable watchdog'),
            ('S1,1', 'Set pin 1 on'),
            ('R1', 'Read pin 1'),
            ('P2,512', 'Set PWM 2 to 50%'),
            ('R2', 'Read pin 2'),
            ('L2', 'Read SSR2 power level'),
            ('W25', 'Set watchdog timeout'),
            ('T10', 'Temperature reading'),
            ('P2,1023', 'Set PWM 2 to 100%'),
            ('R2', 'Read pin 2 again'),
            ('L2', 'Read SSR2 power level'),
            ('P2,0', 'Turn off PWM 2'),
            ('R2', 'Final read pin 2'),
            ('L2', 'Read SSR2 power level'),
            ('D0', 'Disable watchdog')
        ]

    failures = 0
    for cmd, description in tests:
        log_message(f"\nTest: {description}")
        if not send(fd, cmd):
            log_message(f"WARNING: No response for {cmd}")
            failures += 1
        time.sleep(0.1)

    return failures


def main():
    parser = argparse.ArgumentParser(description='Run serial tests on Nano relay firmware')
    parser.add_argument('--port', default='/dev/ttyUSB0',
                       help='Serial port to use (default: /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('--boot-delay', type=float, default=1.2,
                       help='Delay after opening port (default: 1.2s)')
    args = parser.parse_args()

    log_message(f"Opening {args.port} at {args.baud} baud")
    fd = setup_serial(args.port, args.baud)

    # Allow MCU to boot/stabilize
    time.sleep(args.boot_delay)

    try:
        failures = run_test_sequence(fd)
        log_message(f"\nTest complete with {failures} failures")
        sys.exit(1 if failures > 0 else 0)
    except KeyboardInterrupt:
        log_message("\nTest interrupted by user")
        sys.exit(130)
    finally:
        os.close(fd)


if __name__ == '__main__':
    main()
