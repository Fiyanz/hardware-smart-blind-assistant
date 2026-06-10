# AGENTS.md

## Project type
- Single-target PlatformIO project (no workspaces), Arduino framework on ESP32-C3.
- Entrypoint: `src/main.cpp` (includes `Arduino.h`, `WiFiManager`, `ESPAsyncWebServer`, `ArduinoJson`).
- `include/config_page.h` is a read-only embedded HTML config UI for the web server. Do not convert to separate files without keeping the `#include` path stable.

## Hardware / pin constraints
- Buttons: GPIO 2 (UP) and GPIO 3 (DOWN) with INPUT_PULLUP (active-low).
- Serial: 115200 baud, 1s startup delay.
- Bootstrapping WiFi: falls back to AP `SmartBlind-AP` / `12345678`.

## Dependency / build
- Managed by `platformio.ini` lib_deps. Context already cached in `.pio/`.
- ignore `.pio/`, `.vscode/c_cpp_properties.json`, `.vscode/launch.json` in diffs (PlatformIO auto-generated).
- Use `pio run` to build, `pio run -t upload` to flash, `pio device monitor` for serial.

## Config and runtime notes
- `SERVER_URL` in `src/main.cpp` is a stub (`http://your-server.com/api/mode-change`) and must be set to the real backend before first deploy.
- Mode array is `int modeArray[5] = {0,1,2,3,4}`; UP increments modulo 5, DOWN decrements. Keep the modulo boundary in mind when changing `ARRAY_SIZE`.
- WiFi credentials persist via WiFiManager NVS partition; reset via the config page or uncommenting `wifiManager.resetSettings()` during a local upload.

## Tests
- `test/` directory exists but contains no tests yet (only README stub). Do not break its structure if adding tests later.
- Unit tests run as native host-side by PlatformIO Test Runner, not on hardware.

## Style / conventions
- C++ not strictly required, but existing code uses `.cpp` + `ArduinoJson` static document allocation.
- HTTP POST body is JSON `{"mode": N, "timestamp": millis()}` — backend must parse this contract.
- Avoid long blocking `delay()` in `loop()` beyond the existing 10 ms tick; the blind assistant button handlers already use software debouncing.
