## Quick context

- Project type: PlatformIO ESP32 + Arduino project. See `platformio.ini` (env: `esp32_four`).
- Main firmware entry: `src/main.cpp` (touchscreen + TFT display example). The file contains hardware-specific comments and calibration values.
- Library management: private libs live in `lib/`; headers in `include/`. Tests (PlatformIO Test Runner) go under `test/`.

## Priority goals for the agent

- Make small, fully buildable edits by default (preserve PlatformIO settings).
- Prefer non-invasive changes: update `src/` or `lib/` files, add headers to `include/`, and always keep `platformio.ini` env names intact.

## Build / upload / debug commands (explicit)

- Build: `pio run -e esp32_four`
- Upload: `pio run -e esp32_four -t upload`
- Serial monitor: `pio device monitor -e esp32_four` (monitor speed is defined in `platformio.ini` as `monitor_speed = 115200`)
- Unit tests: use PlatformIO test runner (no tests currently present; add under `test/` if needed): `pio test -e <env>`
- Note: hardware debug via `pio debug` may require a debug probe and changes to `platformio.ini`; prefer Serial prints for iterative work.

## Project-specific patterns & gotchas

- Hardware-specific setup is documented inside `src/main.cpp` header comments. The `TFT_eSPI` library requires a custom `User_Setup.h` configured for the display used — do not assume default library wiring.
- Touchscreen calibration: `src/main.cpp` maps raw X/Y ranges with constants (example: `map(p.x, 200, 3700, 1, SCREEN_WIDTH)`). If touches are off, adjust these numeric ranges rather than changing mapping logic.
- Libraries pinned in `platformio.ini`'s `lib_deps` should be preserved when editing build configuration — changes may alter binary size or behavior.
- Private libraries belong in `lib/<name>/src` and may include a `library.json` to tweak LDF behavior.

## When editing code

- Prefer minimal, testable changes. After edits, the agent should run a local build (`pio run -e esp32_four`) to confirm no compile errors.
- If a change touches hardware wiring or pin defines, reference the top-of-file comments in `src/main.cpp` and the `platformio.ini` platform/board entries.
- If adding new dependencies, add them to `platformio.ini` under `lib_deps` and run a build to verify resolution.

## Files to reference for examples

- `platformio.ini` — environment name `esp32_four`, board `esp32dev`, monitor speed and `lib_deps` list.
- `src/main.cpp` — interactive touchscreen/TFT example, shows mapping/calibration, SPI pins, and library usage (TFT_eSPI, XPT2046_Touchscreen).
- `include/README`, `lib/README`, `test/README` — show repo conventions for headers, private libs, and tests.

## Output expectations for the agent

- Provide concise diffs/patches that build cleanly.
- When proposing hardware-related changes, include a short checklist: (1) build passes, (2) serial output sanity check lines added, (3) note any required `User_Setup.h` edits.

---

If any of the hardware wiring, board type, or target environment differs from `platformio.ini`, ask for the specific board/port and a photo of wiring before making changes that touch pins or calibration values.

Please review and tell me if you'd like more examples (e.g., a sample unit test or a recommended `User_Setup.h` snippet to include in the repo).
