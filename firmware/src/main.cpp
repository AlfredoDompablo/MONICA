#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include "secrets.h"

// --- Hardware Definitions for Heltec Wireless Tracker V1.1 ---
#define VEXT_PIN 3        // Powers the GPS and external sensors
#define BACKLIGHT_PIN 21  // Controls LCD Backlight
#define GNSS_RX_PIN 33    // Use PIN 33 for RX (Connects to GPS_TX)
#define GNSS_TX_PIN 34    // Use PIN 34 for TX (Connects to GPS_RX)

// --- Objects ---
TinyGPSPlus gps;
TFT_eSPI tft = TFT_eSPI(); 

// --- Variables ---
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000; // Send data every 10 seconds

void setup() {
  Serial.begin(115200);
  
  // 1. Init Power Systems
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, HIGH); // Enable Vext (Power on GPS)
  
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH); // Turn on Backlight (Active HIGH usually)
  delay(100);

  // 2. Init Display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  tft.setCursor(0, 0);
  tft.println("Heltec Booting...");
  
  // 3. Init GPS
  // Note: We swap pins because ESP_RX receives from GNSS_TX
  Serial1.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  tft.println("GPS Init...");

  // 4. Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.print("Wifi: ");
  Serial.print("Connecting to WiFi");
  
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tft.print(".");
    dots++;
    if (dots > 10) {
        dots = 0;
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.println("Connecting...");
    }
  }

  // Success
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Connected!");
  tft.setTextSize(1);
  tft.println(WiFi.localIP());
  
  Serial.println("");
  Serial.println("WiFi connected.");
}

// Function to read GPS data for a minimal amount of time
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

void sendSensorData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // --- Update Screen ---
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.println(String("Node: ") + NODE_ID);
    
    // Check GPS Fix BEFORE sending
    if (!gps.location.isValid()) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.println("\nNO GPS FIX!");
        tft.setTextSize(1);
        tft.println("\nSearching...");
        tft.print("Sats: ");
        tft.println(gps.satellites.value());
        
        Serial.println("Skipping send: No GPS Fix.");
        return; // EXIT FUNCTION, DO NOT SEND
    }

    // GPS Valid - Proceed to build JSON
    StaticJsonDocument<400> doc;
    doc["node_id"] = NODE_ID;
    
    doc["latitude"] = gps.location.lat();
    doc["longitude"] = gps.location.lng();
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("GPS: FIX ");
    tft.print(gps.satellites.value());
    tft.println(" Sats");
    tft.print("Lat: "); tft.println(gps.location.lat(), 4);
    tft.print("Lng: "); tft.println(gps.location.lng(), 4);

    // Simulate sensor values
    doc["ph"] = random(60, 80) / 10.0;
    doc["dissolved_oxygen"] = random(70, 90) / 10.0;
    doc["turbidity"] = random(0, 50);
    doc["conductivity"] = random(400, 600);
    doc["temperature"] = random(200, 300) / 10.0;
    doc["battery_level"] = random(80, 100);

    String jsonOutput;
    serializeJson(doc, jsonOutput);

    // Send POST request
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); 
    tft.println("Sending data..."); 

    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", API_KEY);
    
    int httpResponseCode = http.POST(jsonOutput);
    
    if (httpResponseCode > 0) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.print("Sent! Code: ");
      tft.println(httpResponseCode);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.print("Error: ");
      tft.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    tft.fillScreen(TFT_RED);
    tft.setCursor(0, 0);
    tft.println("WiFi Lost!");
    WiFi.reconnect();
  }
}

void loop() {
  // Feed GPS parser constantly
  smartDelay(100); 
  
  if (millis() - lastSendTime > sendInterval) {
    sendSensorData();
    lastSendTime = millis();
  }
}
