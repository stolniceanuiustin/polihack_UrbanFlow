#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "TrafficController.h"
#include "WIFI_CREDENTIALS.h"
#include "DEFAULT_JSON_CONFIG.h"
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

#if defined(ESP32)
#define RXD2 16
#define TXD2 17 // We don't use TX, but the function asks for it
#else
#define RXD2 16 // Fallback
#define TXD2 17
#endif

char rxBuffer[256];

// Arduino will send traffic data like this: "pin,val pin,val pin,val"

void parse_traffic_data(Intersection *intr, char *inputString)
{
    char *pairToken = strtok(inputString, " ");

    while (pairToken != NULL)
    {
        char *commaPtr = strchr(pairToken, ',');

        if (commaPtr != NULL)
        {
            // 3. Replace comma with null terminator to split into two strings
            *commaPtr = '\0';

            // pairToken points to the PIN (e.g., "8")
            // commaPtr + 1 points to the VALUE (e.g., "102")
            int incomingPin = atoi(pairToken);
            int incomingVal = atoi(commaPtr + 1);

            // 4. Find the matching Lane in the Graph
            for (uint32_t i = 0; i < intr->lane_cnt; i++)
            {
                if (intr->lanes[i].hw.sensor_pin == incomingPin)
                {
                    intr->lanes[i].current_traffic_value = incomingVal;
                    break; // Stop searching once lane is found
                }
            }
        }

        // 5. Get the next token
        pairToken = strtok(NULL, " ");
    }
}

// TODO. DO a post if config is invalid

bool parseConfig(String jsonPayload)
{
    // 1. Allocate and Deserialize
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, jsonPayload);

    // --- ADDED: PRETTY PRINT ---
    // Only print if deserialization was successful to avoid confusion
    if (!error)
    {
        Serial.println("\n--- Received Configuration (Pretty Print) ---");
        serializeJsonPretty(doc, Serial);
        Serial.println("\n---------------------------------------------");
    }
    else
    {
        // If parsing failed, print the raw payload so you can debug the syntax error
        Serial.println("\n--- Received Invalid JSON ---");
        Serial.println(jsonPayload);
    }
    // ---------------------------

    if (error)
    {
        Serial.print("JSON Parse Error: ");
        Serial.println(error.c_str());
        return false;
    }

    // 2. Init
    intersection_init(&intr, doc["default_phase_duration_ms"]);

    // 3. Parse Lanes
    JsonArray lanes = doc["lanes"];
    for (JsonObject l : lanes)
    {
        LaneHardware hw;
        JsonObject l_hw = l["hw"];

        hw.sensor_pin = l_hw["sensor_pin"];
        hw.green_pin = l_hw["red_pin"];
        hw.yellow_pin = l_hw["yellow_pin"];
        hw.red_pin = l_hw["green_pin"];

        uint16_t bearing = l.containsKey("bearing") ? l["bearing"] : 0;

        add_lane(&intr, l["id"], (LaneType)l["type"].as<int>(), hw, bearing);
    }

    // 4. Parse Connections
    JsonArray connections = doc["connections"];
    for (JsonObject c : connections)
    {
        add_connection(&intr, c["source_lane_idx"], c["target_lane_idx"]);
    }

    // 5. Calculate Physics
    compute_conflicts_on_device(&intr);

    // 6. Parse Phases & Validate
    JsonArray phases = doc["phases"];
    int phaseCount = 0;

    for (JsonObject p : phases)
    {
        uint64_t mask = p["active_connections_mask"];

        if (!is_phase_safe(&intr, mask))
        {
            Serial.printf("FATAL: Cloud requested UNSAFE Phase #%d (Mask: %llu). Skipping.\n", phaseCount);
            continue;
        }

        add_phase(&intr, mask, p["duration_ms"]);
        phaseCount++;
    }

    Serial.printf("Graph Built: %d Lanes, %d Conn, %d Phases\n",
                  intr.lane_cnt, intr.connection_cnt, intr.phase_cnt);

    if (intr.phase_cnt == 0)
    {
        Serial.println("Error: No valid safe phases found.");
        return false;
    }

    return true;
}
void setup()
{
    Serial.begin(115200);
    delay(2000); // Give serial monitor time to catch up
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
    pinMode(HEARTBEAT_PIN, OUTPUT);

    Serial.println("\n--- WiFi Traffic Controller ---");

    bool configSuccess = false;
    bool wifiAvailable = false;

    // --- STEP 1: Attempt WiFi (With Timeout) ---
    // If we don't time out, the board will hang forever if the router is off
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20)
    { // Try for ~10 seconds
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        wifiAvailable = true;
    }
    else
    {
        Serial.println("\nWiFi Timeout. Skipping Cloud fetch.");
    }

    // --- STEP 2: Fetch Config from Cloud (If WiFi is up) ---
    if (wifiAvailable)
    {
        WiFiClientSecure client;
        client.setInsecure(); // IGNORE SSL ERRORS

        HTTPClient http;
        Serial.print("Fetching config from: ");
        Serial.println(serverUrl);

        // Timeout setup for HTTP
        http.setTimeout(5000);
        http.begin(client, serverUrl);

        int httpCode = http.GET();

        if (httpCode == 200)
        {
            String payload = http.getString();
            Serial.println("Cloud Config Received. Parsing...");

            // Try to parse the cloud config
            if (parseConfig(payload))
            {
                Serial.println("SUCCESS: Cloud Config Loaded.");
                configSuccess = true;
            }
            else
            {
                Serial.println("ERROR: Cloud JSON was corrupt or invalid.");
            }
        }
        else
        {
            Serial.printf("HTTP Failed. Error Code: %d\n", httpCode);
        }
        http.end();
    }

    // --- STEP 3: Fallback to Default (If Cloud Failed) ---
    if (!configSuccess)
    {
        Serial.println("\n!!! WARNING: Cloud Config Failed/Unavailable !!!");
        Serial.println("Attempting to load DEFAULT OFFLINE CONFIG...");

        if (parseConfig(DEFAULT_OFFLINE_CONFIG))
        {
            Serial.println("SUCCESS: Default Config Loaded.");
            configSuccess = true;
        }
        else
        {
            Serial.println("CRITICAL ERROR: Default JSON is also invalid.");
        }
    }

    // --- STEP 4: Start Logic or Halt ---
    if (configSuccess)
    {
        Serial.println("--- Starting Traffic Controller ---");
        controller_setup();
    }
    else
    {
        Serial.println("--- SYSTEM HALTED: NO CONFIGURATION ---");
        while (1)
        {
            digitalWrite(HEARTBEAT_PIN, !digitalRead(HEARTBEAT_PIN));
            delay(100);
        }
    }
}
void loop()
{
    if (Serial2.available() > 0) {
        // 1. Read the line until the newline character ('\n')
        int bytesRead = Serial2.readBytesUntil('\n', rxBuffer, sizeof(rxBuffer) - 1);
        
        // 2. If we got data, process it
        if (bytesRead > 0) {
            rxBuffer[bytesRead] = '\0'; // Add null terminator to make it a string
            
            // 3. Update the Graph Physics
            parse_traffic_data(&intr, rxBuffer);
            
            // Optional Debug: Print what we heard
            // Serial.print("Sensor Update: "); Serial.println(rxBuffer);
        }
    }
    
    controller_loop();

    long now = millis();
    if ((now / 500) % 2 == 0)
    {
        digitalWrite(HEARTBEAT_PIN, HIGH);
    }
    else
    {
        digitalWrite(HEARTBEAT_PIN, LOW);
    }
}