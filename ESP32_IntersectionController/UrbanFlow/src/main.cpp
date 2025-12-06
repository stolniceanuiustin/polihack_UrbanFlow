#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "TrafficController.h"
#include "WIFI_CREDENTIALS.h"

#if defined(ESP8266)
    #define HEARTBEAT_PIN 5 
    #define BOARD_NAME "ESP8266"

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define HEARTBEAT_PIN 36    
    #define BOARD_NAME "ESP32-S3"

#elif defined(ESP32)
    #define HEARTBEAT_PIN 23 
    #define BOARD_NAME "ESP32-Standard"

#else
    #error "Unknown Board! Please define HEARTBEAT_PIN manually."
#endif

bool parseConfig(String jsonPayload) {
    // 1. Allocate and Deserialize
    DynamicJsonDocument doc(16384); 
    DeserializationError error = deserializeJson(doc, jsonPayload);

    // --- ADDED: PRETTY PRINT ---
    // Only print if deserialization was successful to avoid confusion
    if (!error) {
        Serial.println("\n--- Received Configuration (Pretty Print) ---");
        serializeJsonPretty(doc, Serial);
        Serial.println("\n---------------------------------------------");
    } else {
        // If parsing failed, print the raw payload so you can debug the syntax error
        Serial.println("\n--- Received Invalid JSON ---");
        Serial.println(jsonPayload); 
    }
    // ---------------------------

    if (error) {
        Serial.print("JSON Parse Error: "); Serial.println(error.c_str());
        return false;
    }

    // 2. Init
    intersection_init(&intr, doc["default_phase_duration_ms"]);

    // 3. Parse Lanes
    JsonArray lanes = doc["lanes"];
    for (JsonObject l : lanes) {
        LaneHardware hw;
        JsonObject l_hw = l["hw"];
        
        hw.sensor_pin = l_hw["sensor_pin"];
        hw.green_pin  = l_hw["red_pin"];     
        hw.yellow_pin = l_hw["yellow_pin"];
        hw.red_pin    = l_hw["green_pin"];   

        uint16_t bearing = l.containsKey("bearing") ? l["bearing"] : 0;

        add_lane(&intr, l["id"], (LaneType)l["type"].as<int>(), hw, bearing);
    }

    // 4. Parse Connections
    JsonArray connections = doc["connections"];
    for (JsonObject c : connections) {
        add_connection(&intr, c["source_lane_idx"], c["target_lane_idx"]);
    }

    // 5. Calculate Physics
    compute_conflicts_on_device(&intr);

    // 6. Parse Phases & Validate
    JsonArray phases = doc["phases"];
    int phaseCount = 0;
    
    for (JsonObject p : phases) {
        uint64_t mask = p["active_connections_mask"];
        
        if (!is_phase_safe(&intr, mask)) {
            Serial.printf("FATAL: Cloud requested UNSAFE Phase #%d (Mask: %llu). Skipping.\n", phaseCount);
            continue; 
        }

        add_phase(&intr, mask, p["duration_ms"]);
        phaseCount++;
    }
    
    Serial.printf("Graph Built: %d Lanes, %d Conn, %d Phases\n", 
                  intr.lane_cnt, intr.connection_cnt, intr.phase_cnt);

    if (intr.phase_cnt == 0) {
        Serial.println("Error: No valid safe phases found.");
        return false;
    }

    return true;
}
void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(HEARTBEAT_PIN, OUTPUT);

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

    long now = millis();
    if ((now / 500) % 2 == 0) {
        digitalWrite(HEARTBEAT_PIN, HIGH);
    } else {
        digitalWrite(HEARTBEAT_PIN, LOW);
    }
}