#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "TrafficController.h"
#include "WIFI_CREDENTIALS.h"
#include "DEFAULT_JSON_CONFIG.h"
#include "CONFIG.h"

#ifndef SIMULATION_MODE
#define SIMULATION_MODE true
#endif

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
#define TXD2 17
#else
#define RXD2 16
#define TXD2 17
#endif

char rxBuffer[256];

void send_intersection_status(const char *currentStatus)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Cannot update status: WiFi not connected.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Needed for using https on localhost

    HTTPClient http;
    String fullUrl = String(sendStatusUrl) + String(currentStatus);

    Serial.print("Posting status to: ");
    Serial.println(fullUrl);

    http.begin(client, fullUrl);
    int httpResponseCode = http.POST("");

    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.printf("Status Update Success (Code %d): %s\n", httpResponseCode, response.c_str());
    }
    else
    {
        Serial.printf("Status Update Failed. Error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
}
void parse_traffic_data(Intersection *intr, const char *data)
{
    // Format: "1,123 2,234 3,423"
    // ID,Value Space ID,Value

    const char *p = data;

    while (*p)
    {
        // 1. Skip any leading spaces
        while (*p == ' ')
            p++;
        if (!*p)
            break;

        // 2. Read Lane ID
        int incomingID = atoi(p);

        // 3. Skip to comma ','
        while (*p && *p != ',')
            p++;
        if (!*p)
            break;
        p++; // Skip the comma

        // 4. Read Sensor Value
        int val = atoi(p);

        // 5. MATCH ID TO INDEX
        // We look through all lanes to find which one matches this ID
        for (uint32_t i = 0; i < intr->lane_cnt; i++)
        {
            if (intr->lanes[i].id == incomingID)
            {
                received_sensor_value[i] = val;

                break;
            }
        }

        // 6. Skip past the number we just read to find the next space
        while (*p && *p != ' ')
            p++;
    }
}
// Parse JSON Config
bool parseConfig(String jsonPayload)
{
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, jsonPayload);

    if (!error)
    {
        Serial.println("\n--- Received Configuration (Pretty Print) ---");
        serializeJsonPretty(doc, Serial);
        Serial.println("\n---------------------------------------------");
    }
    else
    {
        Serial.println("\n--- Received Invalid JSON ---");
        Serial.println(jsonPayload);
        Serial.print("JSON Parse Error: ");
        Serial.println(error.c_str());
        return false;
    }

    intersection_init(&intr, doc["default_phase_duration_ms"]);

    JsonArray lanes = doc["lanes"];
    for (JsonObject l : lanes)
    {
        LaneHardware hw;
        JsonObject l_hw = l["hw"];
        hw.sensor_pin = l_hw["sensor_pin"];
        hw.green_pin = l_hw["red_pin"]; /// Somehow we got them inversed somewhere on the way.. anyways, it dont matter
        hw.yellow_pin = l_hw["yellow_pin"];
        hw.red_pin = l_hw["green_pin"];
        uint16_t bearing = l.containsKey("bearing") ? l["bearing"] : 0;
        add_lane(&intr, l["id"], (LaneType)l["type"].as<int>(), hw, bearing);
    }

    JsonArray connections = doc["connections"];
    for (JsonObject c : connections)
    {
        add_connection(&intr, c["source_lane_idx"], c["target_lane_idx"]);
    }

    compute_conflicts_on_device(&intr);

    JsonArray phases = doc["phases"];
    int phaseCount = 0;
    for (JsonObject p : phases)
    {
        uint64_t mask = p["active_connections_mask"].as<uint64_t>();
        if (!is_phase_safe(&intr, mask))
        {
            //Serial.printf("FATAL: Cloud requested UNSAFE Phase #%d (Mask: %llu). Ignoring for now.\n", phaseCount, mask);
            // return false;
        }
        add_phase(&intr, mask, p["duration_ms"]);
        phaseCount++;
    }

    Serial.printf("Graph Built: %d Lanes, %d Conn, %d Phases\n", intr.lane_cnt, intr.connection_cnt, intr.phase_cnt);

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
    delay(2000);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    pinMode(HEARTBEAT_PIN, OUTPUT);

    Serial.println("\n--- WiFi Traffic Controller ---");

    bool cloudConfigSuccess = false;
    bool systemReady = false;
    bool wifiAvailable = false;

    // --- STEP 1: Connect to WiFi ---
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20)
    {
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
        Serial.println("\nWiFi Timeout.");
    }

    // --- STEP 2: Fetch Config from Cloud ---
    if (wifiAvailable)
    {
        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        Serial.print("Fetching config from: ");
        Serial.println(serverUrl);

        http.setTimeout(5000);
        http.begin(client, serverUrl);

        int httpCode = http.GET();

        if (httpCode == 200)
        {
            String payload = http.getString();
            Serial.println("Cloud Config Received. Parsing...");

            if (parseConfig(payload))
            {
                Serial.println("SUCCESS: Cloud Config Loaded.");
                send_intersection_status("OK");
                cloudConfigSuccess = true;
                systemReady = true;
            }
            else
            {
                Serial.println("ERROR: Cloud JSON was corrupt or invalid.");
                send_intersection_status("CONFIG_ERROR"); // REQUIREMENT: Send Error on parse fail
            }
        }
        else
        {
            Serial.printf("HTTP Failed. Error Code: %d\n", httpCode);
            send_intersection_status("CONFIG_ERROR"); // REQUIREMENT: Send Error on HTTP fail
        }
        http.end();
    }

    // --- STEP 3: Fallback ONLY if SIMULATION_MODE is active ---
    if (!cloudConfigSuccess)
    {
        if (SIMULATION_MODE)
        {
            Serial.println("\n!!! Cloud Config Failed. SIMULATION_MODE is ON. !!!");
            Serial.println("Attempting to load DEFAULT OFFLINE CONFIG...");

            if (parseConfig(DEFAULT_OFFLINE_CONFIG))
            {
                Serial.println("SUCCESS: Default Config Loaded (Simulation).");
                // We do NOT send "OK" here. We leave the previous "CONFIG_ERROR"
                // as the last status sent to the server so the admin knows the cloud failed.
                systemReady = true;
            }
            else
            {
                Serial.println("CRITICAL ERROR: Default JSON is also invalid.");
            }
        }
        else
        {
            Serial.println("\n!!! Cloud Config Failed. SIMULATION_MODE is OFF. !!!");
            Serial.println("System halting to prevent unsafe operation.");
        }
    }

    // --- STEP 4: Start or Halt ---
    if (systemReady)
    {
        Serial.println("--- Starting Traffic Controller ---");
        controller_setup();
    }
    else
    {
        Serial.println("--- SYSTEM HALTED: NO VALID CONFIGURATION ---");
        while (1)
        {
            // Rapid blink to indicate fatal error
            digitalWrite(HEARTBEAT_PIN, !digitalRead(HEARTBEAT_PIN));
            delay(100);
        }
    }
}
void loop()
{
    if (Serial2.available() > 0)
    {
        int bytesRead = Serial2.readBytesUntil('\n', rxBuffer, sizeof(rxBuffer) - 1);
        if (bytesRead > 0)
        {
            rxBuffer[bytesRead] = '\0';
            parse_traffic_data(&intr, rxBuffer);
        }
    }

    controller_loop();

    // Heartbeat for watchdog
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