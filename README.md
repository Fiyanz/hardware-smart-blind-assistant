# Smart Blind Assistant (ESP32-C3)

Firmware untuk mengontrol mode blind/tirai via 2 push button dengan kirim data HTTP.

## Hardware
- ESP32-C3
- Button ACTION (GPIO 2) dan MODE (GPIO 3), aktif LOW (`INPUT_PULLUP`)
- Built-in LED: GPIO 8 (aktif LOW, `LOW` = nyala)

## Build & Upload
```bash
platformio run
platformio run -t upload
platformio device monitor
```

Lihat IP dari Serial Monitor setelah nyambung ke WiFi.

## Konfigurasi
- `SERVER_URL` di `src/main.cpp` default ke stub. Ganti ke endpoint backend-mu sebelum deploy.
- Mode array: `int modeArray[5] = {0,1,2,3,4}` (modulo 5, dikelola via web UI).
- `ARRAY_SIZE` bisa diubah, tapi pastikan batas modulo konsisten.

## Penggunaan
1. Flash firmware
2. Buka browser ke `http://<ip-esp>/`
3. Ubah `serverUrl` dan 5 mode, klik **Save Settings**
4. Config tersimpan di NVS (persisten setelah restart)

## Endpoint Web Server
- `GET /` — config page
- `GET /api/config` — JSON konfigurasi saat ini
- `POST /api/config` — simpan `{"serverUrl": "...", "modes": [..]}`
- `POST /api/reset-wifi` — reset WiFi, ESP restart
- `GET /api/status` — status koneksi, IP, mode aktif

## Ganti WiFi
- Dari config page: tombol **Reset WiFi Settings** (butuh endpoint aktif)
- Atau dari WiFiManager AP: uncomment `wifiManager.resetSettings()`, upload, connect ke AP `SmartBlind-AP` / `12345678`, lalu comment lagi

## Backend Contract
POST body JSON:
```json
{"mode": N, "timestamp": millis()}
```
Server harus parse `mode` sebagai integer.
