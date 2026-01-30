#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Simulation constants
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000; // Send data every 5 seconds

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void sendSensorData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Create JSON document
    StaticJsonDocument<200> doc;
    doc["node_id"] = NODE_ID;
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
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", API_KEY);
    
    int httpResponseCode = http.POST(jsonOutput);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void loop() {
  if (millis() - lastSendTime > sendInterval) {
    sendSensorData();
    lastSendTime = millis();
  }
}
