/**
 * Smart Blind Assistant - ESP32-WROOM BLE Firmware
 * 
 * Firmware untuk komunikasi BLE dengan app Flutter SightAssist.
 * ESP32-WROOM bertindak sebagai BLE peripheral (server) yang mengirim
 * command dari tombol fisik ke app Flutter via BLE notification.
 * 
 * Tombol:
 *   - Tombol 1 (GPIO 21): Voice command     → kirim 0x01
 *   - Tombol 2 (GPIO  5): Next mode         → kirim 0x02
 *   - Tombol 1+2 bersamaan: Emergency stop  → kirim 0x03
 * 
 * Wiring tombol (active LOW, INPUT_PULLUP):
 *   Kaki 1 → GPIO pin ESP32
 *   Kaki 2 → GND
 *   (tidak perlu resistor eksternal)
 * 
 * Power:
 *   Baterai Li-Ion 3.7V → TP4056 → LDO 3.3V → pin 3V3 ESP32
 * 
 * BLE UUIDs (harus cocok dengan app Flutter):
 *   Service:        4fafc201-1fb5-459e-8fcc-c5c9c331914b
 *   Characteristic: beb5483e-36e1-4688-b7f5-ea07361b26a8
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

// ─── Pin Definitions ────────────────────────────────────────
#define BUTTON_VOICE_PIN   21   // Tombol 1: Voice command
#define BUTTON_MODE_PIN     5   // Tombol 2: Next mode

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
        NimBLEDevice::startAdvertising();
    }

    void onDisconnect(NimBLEServer* pServer) override {
        deviceConnected = false;
        Serial.println(">> Device terputus. Menunggu koneksi...");
        NimBLEDevice::startAdvertising();
    }
};

// ─── Send BLE Command ───────────────────────────────────────

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

void handleButtons() {
    unsigned long now = millis();

    bool btn1 = digitalRead(BUTTON_VOICE_PIN) == LOW;  // Active LOW
    bool btn2 = digitalRead(BUTTON_MODE_PIN) == LOW;

    // ── Deteksi tombol baru ditekan ──
    if (btn1 && !voicePressed) {
        if (now - lastVoicePress > DEBOUNCE_MS) {
            voicePressed = true;
            voiceHandled = false;
            lastVoicePress = now;
            Serial.println("Tombol 1 (Voice) ditekan");
        }
    }

    if (btn2 && !modePressed) {
        if (now - lastModePress > DEBOUNCE_MS) {
            modePressed = true;
            modeHandled = false;
            lastModePress = now;
            Serial.println("Tombol 2 (Mode) ditekan");
        }
    }

    // ── Cek combo press ──
    if (voicePressed && modePressed && !comboSent) {
        unsigned long pressGap = (lastVoicePress > lastModePress)
                                  ? (lastVoicePress - lastModePress)
                                  : (lastModePress - lastVoicePress);

        if (pressGap < COMBO_WINDOW_MS) {
            unsigned long earliestPress = min(lastVoicePress, lastModePress);
            if (now - earliestPress >= COMBO_HOLD_MS) {
                Serial.println(">> COMBO: Kedua tombol ditekan bersamaan!");
                sendCommand(CMD_STOP_ALL);
                comboSent = true;
                voiceHandled = true;
                modeHandled = true;
            }
        }
    }

    // ── Handle single button release ──
    if (!btn1 && voicePressed) {
        voicePressed = false;
        if (!voiceHandled && !comboSent) {
            sendCommand(CMD_VOICE);
        }
        voiceHandled = false;
        if (!btn2 && !modePressed) comboSent = false;
    }

    if (!btn2 && modePressed) {
        modePressed = false;
        if (!modeHandled && !comboSent) {
            sendCommand(CMD_NEXT_MODE);
        }
        modeHandled = false;
        if (!btn1 && !voicePressed) comboSent = false;
    }

    if (!btn1 && !btn2) comboSent = false;
}

// ─── Setup ──────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  Smart Blind Assistant - ESP32-WROOM");
    Serial.println("========================================\n");

    // Setup buttons (internal pull-up, active LOW)
    pinMode(BUTTON_VOICE_PIN, INPUT_PULLUP);
    pinMode(BUTTON_MODE_PIN, INPUT_PULLUP);
    Serial.printf("Tombol Voice: GPIO %d\n", BUTTON_VOICE_PIN);
    Serial.printf("Tombol Mode:  GPIO %d\n", BUTTON_MODE_PIN);

    // ── Inisialisasi BLE ──
    Serial.println("\nInisialisasi BLE...");
    NimBLEDevice::init("SightAssist-ESP32");

    // TX power default ESP32-WROOM sudah optimal, tidak perlu di-set manual

    // Buat BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Buat Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Buat Characteristic dengan READ + NOTIFY
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Set initial value
    uint8_t initVal = 0x00;
    pCharacteristic->setValue(&initVal, 1);

    pService->start();

    // ── Setup Advertising ──
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
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
    delay(10);
}