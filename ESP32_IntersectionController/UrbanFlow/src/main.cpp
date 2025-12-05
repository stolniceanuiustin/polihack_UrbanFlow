#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "TrafficController.h"
#include "WIFI_CREDENTIALS.h"


// --- JSON PARSER ---
bool parseConfig(String jsonPayload) {
    // 16KB buffer for safety
    DynamicJsonDocument doc(16384); 
    DeserializationError error = deserializeJson(doc, jsonPayload);

    if (error) {
        Serial.print("JSON Parse Error: "); Serial.println(error.c_str());
        return false;
    }

    // 1. Init
    intersection_init(&intr, doc["default_phase_duration_ms"]);

    // 2. Parse Lanes (Addressing nested 'hw' and snake_case keys)
    JsonArray lanes = doc["lanes"];
    for (JsonObject l : lanes) {
        LaneHardware hw;
        JsonObject l_hw = l["hw"]; // Access nested object
        
        hw.sensor_pin = l_hw["sensor_pin"];
        hw.green_pin  = l_hw["red_pin"];
        hw.yellow_pin = l_hw["yellow_pin"];
        hw.red_pin    = l_hw["green_pin"];

        add_lane(&intr, l["id"], (LaneType)l["type"].as<int>(), hw);
    }

    // 3. Parse Connections
    JsonArray connections = doc["connections"];
    for (JsonObject c : connections) {
        add_connection(&intr, c["source_lane_idx"], c["target_lane_idx"]);
    }

    // 4. Parse Phases (Reading mask directly)
    JsonArray phases = doc["phases"];
    for (JsonObject p : phases) {
        uint64_t mask = p["active_connections_mask"];
        add_phase(&intr, mask, p["duration_ms"]);
    }
    
    Serial.printf("Graph Built: %d Lanes, %d Conn, %d Phases\n", 
                  intr.lane_cnt, intr.connection_cnt, intr.phase_cnt);

    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- WiFi Traffic Controller ---");

    // 1. Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    // 2. Fetch Config
    WiFiClientSecure client;
    client.setInsecure(); // IGNORE SSL ERRORS (Needed for Local HTTPS)
    
    HTTPClient http;
    Serial.print("Fetching config from: "); Serial.println(serverUrl);
    
    http.begin(client, serverUrl);
    int httpCode = http.GET();
    bool configSuccess = false;

    if (httpCode == 200) {
        String payload = http.getString();
        // Serial.println(payload); // Uncomment to see raw JSON
        
        if (parseConfig(payload)) {
            Serial.println("Config Parsed Successfully!");
            configSuccess = true;
        } else {
            Serial.println("JSON Parse Failed (Logic Error).");
        }
    } else {
        Serial.printf("HTTP Failed. Error: %d\n", httpCode);
    }
    http.end();

    // 3. If Config Loaded, Start Controller Logic
    if (configSuccess) {
        controller_setup(); 
    } else {
        Serial.println("System Halted: No Configuration.");
        while(1) delay(100); 
    }
}

void loop() {
    controller_loop();
}