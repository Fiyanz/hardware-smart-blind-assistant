# Smart Blind Assistant (ESP32-C3)

Firmware ESP32-C3 yang mengontrol mode blind/tirai via 2 push button dengan mengirim perintah BLE ke aplikasi SightAssist.

## Hardware
- ESP32-C3 (misal: ESP32-C3 SuperMini / DevKitM-1)
- Button ACTION: GPIO 2 (`INPUT_PULLUP`, aktif LOW — tekan = LOW, lepas = HIGH)
- Button MODE: GPIO 3 (`INPUT_PULLUP`, aktif LOW — tekan = LOW, lepas = HIGH)
- Built-in LED: GPIO 8 (aktif LOW, `LOW` = nyala, `HIGH` = mati)
- BLE 5.0 built-in, tidak perlu modul tambahan

## Build & Flash
```bash
# Build firmware production (BLE)
pio run -e esp32-c3

# Flash
pio run -e esp32-c3 -t upload

# Serial monitor
pio device monitor -e esp32-c3

# Build test button reader (tanpa BLE)
pio run -e esp32-c3-test

# Flash test
pio run -e esp32-c3-test -t upload

# Monitor test
pio device monitor -e esp32-c3-test
```

## BLE Protocol
- Device name: `SightAssist-ESP32`
- Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- Trigger characteristic UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- Mode: `NOTIFY` (ESP32 kirim notification ke smartphone)
- Data: 1 byte per command

## Perintah Button
| Aksi | Pin | Command | Byte |
|------|-----|---------|------|
| Tekan ACTION | GPIO 2 | CMD_ACTION | `0x01` |
| Tekan MODE | GPIO 3 | CMD_NEXT_MODE | `0x02` |
| Tekan kedua tombol bersamaan | GPIO 2 + 3 | CMD_STOP_ALL | `0x03` |

## Cara Kerja
1. Setelah boot, ESP32 langsung mulai advertising BLE
2. Aplikasi SightAssist scan dan connect ke `SightAssist-ESP32`
3. Aplikasi **subscribe** characteristic `beb5483e-36e1-4688-b7f5-ea07361b26a8` untuk menerima notification
4. Setiap kali tombol ditekan, ESP32 kirim 1 byte notification ke aplikasi
5. LED nyala saat connected, mati saat disconnected. LED off sebentar (50ms) sebagai feedback setiap command dikirim.

## Button Behavior
- **Debounce**: 250 ms (software)
- **Loop tick**: 10 ms (non-blocking)
- **Reconnect delay**: 500 ms setelah disconnect

## Testing Button
File `src/test_button.cpp` adalah firmware test terpisah untuk verifikasi button hardware:
- Menampilkan state HIGH/LOW setiap 500ms
- Gunakan environment `esp32-c3-test` (bukan `esp32-c3`)
- Tanpa BLE, fokus pada pembacaan pin mentah

```bash
pio run -e esp32-c3-test -t upload
pio device monitor -e esp32-c3-test
```

Expected output (tanpa tombol ditekan):
```
ACTION=LOW | MODE=LOW
```

Expected output (tombol ditekan):
```
ACTION=HIGH | MODE=LOW    ← ACTION ditekan
ACTION=LOW | MODE=LOW     ← ACTION dilepas
```

## Troubleshooting
- Jika tidak ada output di serial: tekan Ctrl+T lalu Ctrl+H di monitor untuk help, pastikan port dan baud rate (115200) benar
- Jika button terdeteksi tapi tidak kirim BLE: pastikan aplikasi sudah connect dan subscribe ke characteristic
- Untuk board yang tidak auto-download mode: tahan BOOT saat plug USB-C, lalu release
