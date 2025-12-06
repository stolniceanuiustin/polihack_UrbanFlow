#include "TrafficController.h"
#include "CONFIG.h"

#define DEBUG false

// --- GLOBALS ---
Intersection intr; 

// --- CONSTANTS ---
const uint32_t MIN_GREEN_TIME = 1000;
const uint32_t MAX_GREEN_TIME = 50000;
const uint32_t STARVATION_THRESHOLD = 99999;
const uint32_t SIMULATE_TRAFFIC_INTERVAL = 100;
const uint32_t DECISION_TIME_INTERVAL = 1000;

// --- STATE ---
unsigned long last_decision_time = 0;
unsigned long last_simulation_time = 0;
unsigned long current_phase_start_time = 0;
uint32_t current_phase_idx = 0;
unsigned long phase_last_serviced[4] = {0, 0, 0, 0};

// --- HARDWARE HELPERS (Private) ---

void set_lights(const Lane *lane, bool red, bool yellow, bool green) {
    if (lane->hw.red_pin != -1) digitalWrite(lane->hw.red_pin, red ? HIGH : LOW);
    if (lane->hw.yellow_pin != -1) digitalWrite(lane->hw.yellow_pin, yellow ? HIGH : LOW);
    if (lane->hw.green_pin != -1) digitalWrite(lane->hw.green_pin, green ? HIGH : LOW);
}

void initialize_hardware(Intersection *intr) {
    for (uint32_t i = 0; i < intr->lane_cnt; i++) {
        const Lane *lane = &intr->lanes[i];
        if (lane->hw.red_pin != -1) { pinMode(lane->hw.red_pin, OUTPUT); digitalWrite(lane->hw.red_pin, HIGH); }
        if (lane->hw.yellow_pin != -1) { pinMode(lane->hw.yellow_pin, OUTPUT); digitalWrite(lane->hw.yellow_pin, LOW); }
        if (lane->hw.green_pin != -1) { pinMode(lane->hw.green_pin, OUTPUT); digitalWrite(lane->hw.green_pin, LOW); }
        if (lane->hw.sensor_pin != -1) { pinMode(lane->hw.sensor_pin, INPUT); }
    }
}

void apply_phase_lights(Intersection *intr, int phase_idx) {
    const Phase *next_phase = &intr->phases[phase_idx];
    for (uint32_t i = 0; i < intr->lane_cnt; i++) {
        Lane *lane = &intr->lanes[i];
        if (lane->type != LANE_IN) continue;

        bool is_green = false;
        for (uint32_t c = 0; c < intr->connection_cnt; c++) {
            if (intr->connections[c].source_lane_idx == i) {
                if (next_phase->active_connections_mask & (1ULL << c)) {
                    is_green = true;
                    break;
                }
            }
        }
        if (is_green) set_lights(lane, false, false, true);
        else set_lights(lane, true, false, false);
    }
}

// --- LOGIC FUNCTIONS (Simulation & Max Pressure) ---

void simulate_traffic_changes(Intersection *intr) {
    if (DEBUG) Serial.println("\n--- Sensor Readings ---");
    
    // 1. Simulate Arrivals (Randomly add cars)
    for (uint32_t i = 0; i < intr->lane_cnt; i++) {
        if (intr->lanes[i].type == LANE_IN && intr->lanes[i].hw.sensor_pin != -1) {
            int sensor_val = analogRead(intr->lanes[i].hw.sensor_pin);
            // Map sensor value to probability (0 to 4095 -> 5% to 100%)
            int arrival_probability = map(sensor_val, 0, 4095, 5, 100);
            
            if (random(0, 100) < arrival_probability) {
                intr->lanes[i].current_traffic_value++;
                if (intr->lanes[i].current_traffic_value > 255) intr->lanes[i].current_traffic_value = 255;
            }
        }
    }

    // 2. Simulate Departures (Remove cars from Green lanes)
    const Phase *curr = &intr->phases[current_phase_idx];
    for (uint32_t c = 0; c < intr->connection_cnt; c++) {
        if (curr->active_connections_mask & (1ULL << c)) {
            uint32_t source_idx = intr->connections[c].source_lane_idx;
            if (intr->lanes[source_idx].current_traffic_value > 0) {
                if (random(0, 100) < 50) intr->lanes[source_idx].current_traffic_value--;
            }
        }
    }
}

//keep in mind that calculate 
int32_t calculate_phase_pressure(Intersection *intr, int phase_index) {
    int32_t pressure = 0;
    const Phase *p = &intr->phases[phase_index];
    for (uint32_t c = 0; c < intr->connection_cnt; c++) {
        if (p->active_connections_mask & (1ULL << c)) {
            uint32_t source_idx = intr->connections[c].source_lane_idx;
            pressure += intr->lanes[source_idx].current_traffic_value;
        }
    }
    return pressure;
}

void run_max_pressure_decision(Intersection *intr) {
    int best_phase_idx = -1;
    unsigned long now = millis();
    unsigned long current_duration = now - current_phase_start_time;

    Serial.println("\n--- Decision Time ---");

    // 1. CHECK TOTAL SYSTEM PRESSURE
    // If no cars are waiting anywhere, we behave like a simple timer
    int32_t total_system_pressure = 0;
    for (uint32_t i = 0; i < intr->phase_cnt; i++) {
        total_system_pressure += calculate_phase_pressure(intr, i);
    }

    if (total_system_pressure == 0) {
        // --- IDLE MODE (Cycle based on Duration) ---
        Serial.println(">> No Traffic Detected. Using Timer Logic.");
        
        uint32_t target_duration = intr->phases[current_phase_idx].duration_ms;
        if (target_duration == 0) target_duration = intr->default_phase_duration_ms;

        if (current_duration >= target_duration) {
            // Cycle to the next phase sequentially
            best_phase_idx = (current_phase_idx + 1) % intr->phase_cnt;
        } else {
            // Keep waiting
            best_phase_idx = current_phase_idx;
        }
    } 
    else {
        // --- MAX PRESSURE MODE (Smart Logic) ---
        
        // Starvation check
        int starved_phase = -1;
        unsigned long max_wait_time = 0;
        
        for (uint32_t i = 0; i < intr->phase_cnt; i++) {
            if (i == current_phase_idx) continue;
            
            if (calculate_phase_pressure(intr, i) > 0) {
                unsigned long wait_time = now - phase_last_serviced[i];
                if (wait_time > STARVATION_THRESHOLD) {
                    if (wait_time > max_wait_time) {
                        max_wait_time = wait_time;
                        starved_phase = i;
                    }
                }
            }
        }

        if (starved_phase != -1) {
            best_phase_idx = starved_phase;
        } else {
            int32_t max_pressure = -1;
            bool force_switch = false;
            
            if (current_duration >= MAX_GREEN_TIME) force_switch = true;
            
            for (uint32_t i = 0; i < intr->phase_cnt; i++) {
                if (force_switch && i == current_phase_idx) continue;
                
                int32_t p = calculate_phase_pressure(intr, i);
                Serial.printf("Phase %d Pressure: %d\n", i, p);
                
                if (p > max_pressure) {
                    max_pressure = p;
                    best_phase_idx = i;
                }
            }
            
            if (!force_switch && max_pressure == calculate_phase_pressure(intr, current_phase_idx)) {
                best_phase_idx = current_phase_idx;
            }
        }
    }

    // --- APPLY DECISION ---
    if (best_phase_idx == -1) best_phase_idx = current_phase_idx;
    
    if (best_phase_idx != current_phase_idx) {
        Serial.printf(">>> SWITCHING: Phase %d -> Phase %d\n", current_phase_idx, best_phase_idx);
        current_phase_idx = best_phase_idx;
        current_phase_start_time = now;
        phase_last_serviced[current_phase_idx] = now;
        apply_phase_lights(intr, current_phase_idx);
    } else {
        Serial.printf(">>> EXTENDING: Phase %d (Time: %lu ms)\n", current_phase_idx, current_duration);
    }
    
    last_decision_time = now;
}

// --- PUBLIC API ---

void controller_setup() {
    Serial.println("--- Initializing Hardware from Config ---");
    initialize_hardware(&intr);

    unsigned long now = millis();
    current_phase_start_time = now;
    for (int i = 0; i < 4; i++) phase_last_serviced[i] = now;

    apply_phase_lights(&intr, current_phase_idx);
}

void controller_loop() {
    if (intr.lane_cnt == 0) return;

    unsigned long now = millis();

    // --- 1. SIMULATION (Only if enabled in CONFIG.h) ---
    if (SIMULATION_MODE) {
        if (now - last_simulation_time > SIMULATE_TRAFFIC_INTERVAL) {
            simulate_traffic_changes(&intr);
            last_simulation_time = now;
        }
    }

    // --- 2. DECISION LOGIC ---
    if (now - last_decision_time > DECISION_TIME_INTERVAL) {
        if (now - current_phase_start_time > MIN_GREEN_TIME) {
            run_max_pressure_decision(&intr);
        }
    }
}