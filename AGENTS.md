# AGENTS.md

## Project type
- Single-target PlatformIO project (no workspaces), Arduino framework on ESP32-C3.
- Entrypoint: `src/main.cpp`.
- Firmware target: ESP32-C3 BLE controller for SightAssist.

## Hardware / pin constraints
- ESP32-C3 Core / SuperMini with BLE 5.0 built-in.
- Button ACTION: GPIO 2 with `INPUT_PULLUP`, active-LOW.
- Button MODE: GPIO 3 with `INPUT_PULLUP`, active-LOW.
- Built-in LED: GPIO 8, active-low (`LOW` = on, `HIGH` = off).
- Serial: 115200 baud, 1s startup delay.
- Both buttons pressed together means `CMD_STOP_ALL`.

## BLE protocol
- Device name: `SightAssist-ESP32`.
- Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`.
- Trigger characteristic UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a8`.
- Characteristic mode: `NOTIFY` with `BLE2902` descriptor.
- Commands sent as 1 byte notification:
  - `0x01` / `CMD_ACTION` — trigger active SightAssist action.
  - `0x02` / `CMD_NEXT_MODE` — cycle to next mode.
  - `0x03` / `CMD_STOP_ALL` — stop all processes.
- SightAssist app must subscribe to notifications for the trigger characteristic.

## Dependency / build
- BLE APIs are provided by the ESP32 Arduino framework; do not add WiFiManager, ESPAsyncWebServer, AsyncTCP, or ArduinoJson unless a future feature needs them.
- Use `pio run` to build, `pio run -t upload` to flash, and `pio device monitor` for serial.
- Ignore `.pio/`, `.vscode/c_cpp_properties.json`, and `.vscode/launch.json` in diffs because PlatformIO generates them.

## Runtime behavior
- BLE advertising starts automatically after boot.
- LED turns on when connected and off when disconnected.
- A sent command briefly turns the LED off for feedback, then restores the connected/off state.
- Button debounce is software-based at 250 ms.
- `loop()` should stay non-blocking with a 10 ms tick; avoid long blocking `delay()` in the main loop.
- After BLE disconnect, firmware re-advertises after 500 ms.

## Upload / board notes
- Board: `esp32-c3-devkitm-1`.
- Flash mode: DIO.
- Upload speed: 921600.
- Monitor speed: 115200.
- USB CDC on boot is enabled for serial output on ESP32-C3 USB-native boards.
- For SuperMini/Core boards that do not enter download mode automatically, hold BOOT while plugging USB-C, then release BOOT.

## Tests
- `test/` directory exists but contains no tests yet (only README stub). Do not break its structure if adding tests later.
- Unit tests run as native host-side by PlatformIO Test Runner, not on hardware.

## Style / conventions
- Use C++ with Arduino framework conventions.
- Keep pin constants and BLE UUIDs in `src/main.cpp`.
- Keep the BLE notification contract as exactly 1 byte.
- Avoid changing `include/config_page.h`; if present, treat it as read-only embedded HTML and keep its include path stable.
