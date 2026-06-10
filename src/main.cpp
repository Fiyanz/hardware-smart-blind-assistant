#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "config_page.h"

// Pin definitions for ESP32-C3
#define BUTTON_UP_PIN    2    // Button to increment mode
#define BUTTON_DOWN_PIN  3    // Button to decrement mode
#define DEBOUNCE_TIME    50   // ms

// Configuration
#define SERVER_URL "http://your-server.com/api/mode-change"
#define ARRAY_SIZE 5

// Global variables - Mode array for Android remote control
int modeArray[ARRAY_SIZE] = {0, 1, 2, 3, 4};  // Mode values to send to Android
int currentMode = 0;
unsigned long lastUpButtonPress = 0;
unsigned long lastDownButtonPress = 0;
bool wifiConnected = false;

Preferences preferences;
AsyncWebServer server(80);
WiFiManager wifiManager;

// Function declarations
void setupButtons();
void handleButtonPress();
void sendModeToServer(int mode);
void setupWebServer();
void loadConfig();
void saveConfig();
String getConfigJson();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\nESP32-C3 Smart Blind Assistant Starting...");
  
  // Setup Preferences (NVS)
  preferences.begin("blind-config", false);
  
  // Setup buttons
  setupButtons();
  
  // Setup WiFi Manager
  // Uncomment to reset WiFi settings
  // wifiManager.resetSettings();
  
  // Auto-connect or start config portal
  if (!wifiManager.autoConnect("SmartBlind-AP", "12345678")) {
    Serial.println("Failed to connect WiFi");
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    
    // Setup web server for config management
    loadConfig();
    setupWebServer();
    server.begin();
    Serial.println("Web server started on port 80");
  }
}

void loop() {
  // Check if WiFi is still connected
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("WiFi disconnected!");
    delay(1000);
    return;
  }
  
  handleButtonPress();
  delay(10);
}

void setupButtons() {
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  Serial.println("Buttons setup complete");
}

void handleButtonPress() {
  unsigned long currentTime = millis();
  
  // Check UP button (next mode)
  if (digitalRead(BUTTON_UP_PIN) == LOW) {
    if (currentTime - lastUpButtonPress >= DEBOUNCE_TIME) {
      currentMode = (currentMode + 1) % ARRAY_SIZE;
      Serial.print("UP pressed - Mode: ");
      Serial.println(modeArray[currentMode]);
      
      sendModeToServer(modeArray[currentMode]);
      lastUpButtonPress = currentTime;
      delay(200);  // Debounce delay
    }
  }
  
  // Check DOWN button (previous mode)
  if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
    if (currentTime - lastDownButtonPress >= DEBOUNCE_TIME) {
      currentMode = (currentMode - 1 + ARRAY_SIZE) % ARRAY_SIZE;
      Serial.print("DOWN pressed - Mode: ");
      Serial.println(modeArray[currentMode]);
      
      sendModeToServer(modeArray[currentMode]);
      lastDownButtonPress = currentTime;
      delay(200);  // Debounce delay
    }
  }
}

void sendModeToServer(int mode) {
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot send mode");
    return;
  }
  
  HTTPClient http;
  
  Serial.print("Sending mode to Android: ");
  Serial.println(mode);
  
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload for mode change
  StaticJsonDocument<200> doc;
  doc["mode"] = mode;
  doc["timestamp"] = millis();
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  Serial.print("Payload: ");
  Serial.println(jsonPayload);
  
  // Send POST request
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void setupWebServer() {
  // Serve config page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", getConfigPageHtml());
  });

  // API: Get config as JSON
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getConfigJson());
  });

  // API: Save config
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
    String body;
    if (request->hasParam("body", true)) {
      body = request->getParam("body", true)->value();
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    // Save server URL
    if (doc.containsKey("serverUrl")) {
      String serverUrl = doc["serverUrl"].as<String>();
      preferences.putString("serverUrl", serverUrl);
    }
    
    // Save mode array
    if (doc.containsKey("modes")) {
      JsonArray modesArray = doc["modes"].as<JsonArray>();
      int modes[5];
      for (int i = 0; i < 5 && i < modesArray.size(); i++) {
        modes[i] = modesArray[i];
      }
      preferences.putBytes("modes", modes, sizeof(modes));
      loadConfig();
    }
    
    String response = "{\"status\":\"ok\"}";
    request->send(200, "application/json", response);
    Serial.println("Config saved successfully");
  });

  // API: Reset WiFi settings
  server.on("/api/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"status\":\"resetting\"}");
    Serial.println("Resetting WiFi settings...");
    delay(500);
    wifiManager.resetSettings();
    ESP.restart();
  });

  // API: Get current status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = WiFi.localIP().toString();
    doc["currentMode"] = currentMode;
    JsonArray arr = doc.createNestedArray("modeArray");
    for (int i = 0; i < ARRAY_SIZE; i++) {
      arr.add(modeArray[i]);
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
}

void loadConfig() {
  // Load server URL
  String serverUrl = preferences.getString("serverUrl", "http://your-server.com/api/mode-change");
  
  // Load mode array
  int loadedModes[ARRAY_SIZE];
  size_t loadedSize = preferences.getBytesLength("modes");
  
  if (loadedSize == sizeof(loadedModes)) {
    preferences.getBytes("modes", loadedModes, sizeof(loadedModes));
    for (int i = 0; i < ARRAY_SIZE; i++) {
      modeArray[i] = loadedModes[i];
    }
  }
  
  // Update current mode if out of bounds
  if (currentMode >= ARRAY_SIZE) {
    currentMode = 0;
  }
  
  Serial.print("Loaded server URL: ");
  Serial.println(serverUrl);
  Serial.print("Loaded modes: ");
  for (int i = 0; i < ARRAY_SIZE; i++) {
    Serial.print(modeArray[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void saveConfig() {
  preferences.putBytes("modes", modeArray, sizeof(modeArray));
  Serial.println("Config saved to NVS");
}

String getConfigJson() {
  StaticJsonDocument<512> doc;
  
  // Read server URL from preferences
  String serverUrl = preferences.getString("serverUrl", "http://your-server.com/api/mode-change");
  
  doc["serverUrl"] = serverUrl;
  
  JsonArray modesArray = doc.createNestedArray("modes");
  for (int i = 0; i < 5; i++) {
    modesArray.add(modeArray[i]);
  }
  
  String json;
  serializeJson(doc, json);
  return json;
}
