/**
 * Smart Blind Assistant - ESP32-C3 BLE Firmware
 * 
 * Firmware untuk komunikasi BLE dengan app Flutter SightAssist.
 * ESP32-C3 bertindak sebagai BLE peripheral (server) yang mengirim
 * command dari tombol fisik ke app Flutter via BLE notification.
 * 
 * Tombol:
 *   - Tombol 1 (GPIO 2): Voice command     → kirim 0x01
 *   - Tombol 2 (GPIO 3): Next mode          → kirim 0x02
 *   - Tombol 1+2 bersamaan: Emergency stop  → kirim 0x03
 * 
 * BLE UUIDs (harus cocok dengan app Flutter):
 *   Service:        4fafc201-1fb5-459e-8fcc-c5c9c331914b
 *   Characteristic: beb5483e-36e1-4688-b7f5-ea07361b26a8
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

// ─── Pin Definitions ────────────────────────────────────────
#define BUTTON_VOICE_PIN   2    // Tombol 1: Voice command
#define BUTTON_MODE_PIN    3    // Tombol 2: Next mode

// ─── BLE UUIDs (HARUS SAMA dengan app Flutter) ─────────────
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ─── BLE Commands ───────────────────────────────────────────
#define CMD_VOICE     0x01    // Voice command (STT)
#define CMD_NEXT_MODE 0x02    // Ganti mode (cycle)
#define CMD_STOP_ALL  0x03    // Emergency stop

// ─── Timing Constants ───────────────────────────────────────
#define DEBOUNCE_MS       50     // Debounce time (ms)
#define COMBO_WINDOW_MS   200    // Window untuk deteksi combo press (ms)
#define COMBO_HOLD_MS     300    // Berapa lama kedua tombol harus ditekan (ms)

// ─── Global Variables ───────────────────────────────────────
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// Button state tracking
unsigned long lastVoicePress = 0;
unsigned long lastModePress = 0;
bool voicePressed = false;
bool modePressed = false;
bool comboSent = false;       // Mencegah kirim combo berulang
bool voiceHandled = false;    // Mencegah kirim voice setelah combo
bool modeHandled = false;     // Mencegah kirim mode setelah combo

// ─── BLE Server Callbacks ───────────────────────────────────

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        deviceConnected = true;
        Serial.println(">> Device terhubung!");
        // Lanjutkan advertising agar bisa reconnect
        NimBLEDevice::startAdvertising();
    }

    void onDisconnect(NimBLEServer* pServer) override {
        deviceConnected = false;
        Serial.println(">> Device terputus. Menunggu koneksi...");
        // Mulai advertising lagi
        NimBLEDevice::startAdvertising();
    }
};

// ─── Send BLE Command ───────────────────────────────────────

/**
 * Kirim command byte ke app Flutter via BLE notification.
 * Hanya kirim jika ada device yang terhubung.
 */
void sendCommand(uint8_t cmd) {
    if (!deviceConnected) {
        Serial.printf("Tidak ada koneksi BLE. Command 0x%02X diabaikan.\n", cmd);
        return;
    }

    pCharacteristic->setValue(&cmd, 1);
    pCharacteristic->notify();
    
    const char* cmdName;
    switch (cmd) {
        case CMD_VOICE:     cmdName = "VOICE"; break;
        case CMD_NEXT_MODE: cmdName = "NEXT_MODE"; break;
        case CMD_STOP_ALL:  cmdName = "STOP_ALL"; break;
        default:            cmdName = "UNKNOWN"; break;
    }
    Serial.printf(">> Kirim command: 0x%02X (%s)\n", cmd, cmdName);
}

// ─── Button Handling ────────────────────────────────────────

/**
 * Baca status tombol dengan debounce dan deteksi combo.
 * 
 * Logika:
 * 1. Jika kedua tombol ditekan bersamaan (dalam COMBO_WINDOW_MS) 
 *    dan ditahan >= COMBO_HOLD_MS → kirim STOP_ALL (0x03)
 * 2. Jika hanya tombol 1 → kirim VOICE (0x01)
 * 3. Jika hanya tombol 2 → kirim NEXT_MODE (0x02)
 */
void handleButtons() {
    unsigned long now = millis();
    
    bool btn1 = digitalRead(BUTTON_VOICE_PIN) == LOW;  // Active LOW (INPUT_PULLUP)
    bool btn2 = digitalRead(BUTTON_MODE_PIN) == LOW;
    
    // ── Deteksi tombol baru ditekan (falling edge) ──
    if (btn1 && !voicePressed) {
        // Tombol 1 baru ditekan
        if (now - lastVoicePress > DEBOUNCE_MS) {
            voicePressed = true;
            voiceHandled = false;
            lastVoicePress = now;
            Serial.println("Tombol 1 (Voice) ditekan");
        }
    }
    
    if (btn2 && !modePressed) {
        // Tombol 2 baru ditekan
        if (now - lastModePress > DEBOUNCE_MS) {
            modePressed = true;
            modeHandled = false;
            lastModePress = now;
            Serial.println("Tombol 2 (Mode) ditekan");
        }
    }
    
    // ── Cek combo press (kedua tombol ditekan bersamaan) ──
    if (voicePressed && modePressed && !comboSent) {
        // Kedua tombol ditekan — cek apakah dalam window combo
        unsigned long pressGap = (lastVoicePress > lastModePress) 
                                  ? (lastVoicePress - lastModePress) 
                                  : (lastModePress - lastVoicePress);
        
        if (pressGap < COMBO_WINDOW_MS) {
            // Kedua tombol ditekan hampir bersamaan
            unsigned long earliestPress = min(lastVoicePress, lastModePress);
            if (now - earliestPress >= COMBO_HOLD_MS) {
                // Ditahan cukup lama → kirim STOP_ALL
                Serial.println(">> COMBO: Kedua tombol ditekan bersamaan!");
                sendCommand(CMD_STOP_ALL);
                comboSent = true;
                voiceHandled = true;  // Jangan kirim voice/mode lagi
                modeHandled = true;
            }
        }
    }
    
    // ── Handle single button release ──
    if (!btn1 && voicePressed) {
        // Tombol 1 dilepas
        voicePressed = false;
        if (!voiceHandled && !comboSent) {
            // Single press → kirim VOICE
            sendCommand(CMD_VOICE);
        }
        voiceHandled = false;
        
        // Reset combo flag jika kedua tombol sudah dilepas
        if (!btn2 && !modePressed) {
            comboSent = false;
        }
    }
    
    if (!btn2 && modePressed) {
        // Tombol 2 dilepas
        modePressed = false;
        if (!modeHandled && !comboSent) {
            // Single press → kirim NEXT_MODE
            sendCommand(CMD_NEXT_MODE);
        }
        modeHandled = false;
        
        // Reset combo flag jika kedua tombol sudah dilepas
        if (!btn1 && !voicePressed) {
            comboSent = false;
        }
    }
    
    // Reset combo jika kedua tombol sudah dilepas
    if (!btn1 && !btn2) {
        comboSent = false;
    }
}

// ─── Setup ──────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  Smart Blind Assistant - ESP32-C3 BLE");
    Serial.println("========================================\n");
    
    // Setup buttons (internal pull-up, active LOW)
    pinMode(BUTTON_VOICE_PIN, INPUT_PULLUP);
    pinMode(BUTTON_MODE_PIN, INPUT_PULLUP);
    Serial.printf("Tombol Voice: GPIO %d\n", BUTTON_VOICE_PIN);
    Serial.printf("Tombol Mode:  GPIO %d\n", BUTTON_MODE_PIN);
    
    // ── Inisialisasi BLE ──
    Serial.println("\nInisialisasi BLE...");
    NimBLEDevice::init("SightAssist-ESP32");
    
    // Set TX power untuk jangkauan optimal
    NimBLEDevice::setPower(ESP_PWR_LVL_P21);
    
    // Buat BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    // Buat Service dengan UUID yang sesuai app Flutter
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
    // Buat Characteristic dengan kemampuan READ + NOTIFY
    // App Flutter subscribe ke notification ini untuk menerima command
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    // Set initial value
    uint8_t initVal = 0x00;
    pCharacteristic->setValue(&initVal, 1);
    
    // Start service
    pService->start();
    
    // ── Setup Advertising ──
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    
    // Start advertising
    NimBLEDevice::startAdvertising();
    
    Serial.println("\nBLE siap! Menunggu koneksi dari app Flutter...");
    Serial.printf("Device name: SightAssist-ESP32\n");
    Serial.printf("Service UUID: %s\n", SERVICE_UUID);
    Serial.printf("Char UUID:    %s\n", CHARACTERISTIC_UUID);
    Serial.println("\nTekan tombol untuk mengirim command:");
    Serial.println("  Tombol 1       → Voice (0x01)");
    Serial.println("  Tombol 2       → Next Mode (0x02)");
    Serial.println("  Tombol 1+2     → Stop All (0x03)");
    Serial.println("----------------------------------------\n");
}

// ─── Main Loop ──────────────────────────────────────────────

void loop() {
    handleButtons();
    delay(10);  // Small delay untuk mengurangi CPU usage
}
