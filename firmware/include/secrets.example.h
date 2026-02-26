#ifndef SECRETS_H
#define SECRETS_H

// WiFi Credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID_HERE";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD_HERE";

// API Configuration
// IMPORTANT: Use your computer's local IP, not localhost
const char* API_URL = "http://192.168.1.X:3000/api/sensor-readings";

// Security
// Generate this via the web dashboard
const char* API_KEY = "YOUR_GENERATED_API_KEY_HERE";

// Node Configuration
const char* NODE_ID = "ESP32_NODE_01";

#endif
