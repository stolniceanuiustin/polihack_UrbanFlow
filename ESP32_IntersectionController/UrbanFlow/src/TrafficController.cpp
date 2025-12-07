#include "TrafficController.h"
#include "CONFIG.h"

#define DEBUG false  // Set to true for detailed Sensor readings

// --- GLOBALS ---
Intersection intr; 

// --- CONSTANTS ---
const uint32_t MIN_GREEN_TIME = 5000;
const uint32_t MAX_GREEN_TIME = 50000;
const uint32_t STARVATION_THRESHOLD = 99999;
const uint32_t SIMULATE_TRAFFIC_INTERVAL = 500;
const uint32_t DECISION_TIME_INTERVAL = 1000;

// --- TRANSITION CONSTANTS ---
const uint32_t YELLOW_DURATION_MS = 2000;      
const uint32_t PEDESTRIAN_DURATION_MS = 5000;  
const uint32_t PEDESTRIAN_COOLDOWN_MS = 60000; 

// --- ENUMS ---
enum ControllerState {
    STATE_GREEN_RUNNING,
    STATE_YELLOW_TRANSITION,
    STATE_PEDESTRIAN_RED
};

// --- STATE ---
unsigned long last_decision_time = 0;
unsigned long last_simulation_time = 0;
unsigned long current_phase_start_time = 0;
unsigned long transition_start_time = 0; 

unsigned long last_pedestrian_time = 0; 

uint32_t current_phase_idx = 0;
uint32_t next_pending_phase_idx = 0;     
int      phase_change_counter = 0;       
ControllerState current_state = STATE_GREEN_RUNNING;

unsigned long phase_last_serviced[4] = {0, 0, 0, 0};

uint16_t received_sensor_value[64];   

// --- HARDWARE HELPERS ---

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

// 1. NORMAL GREEN
void apply_phase_lights_green(Intersection *intr, int phase_idx) {
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
        if (is_green) set_lights(lane, false, false, true); // Green ON
        else set_lights(lane, true, false, false);          // Red ON
    }
}

// 2. TRANSITION YELLOW
void apply_transition_yellow(Intersection *intr, int phase_idx_ending) {
    const Phase *ending_phase = &intr->phases[phase_idx_ending];
    
    for (uint32_t i = 0; i < intr->lane_cnt; i++) {
        Lane *lane = &intr->lanes[i];
        if (lane->type != LANE_IN) continue;

        bool was_green = false;
        for (uint32_t c = 0; c < intr->connection_cnt; c++) {
            if (intr->connections[c].source_lane_idx == i) {
                if (ending_phase->active_connections_mask & (1ULL << c)) {
                    was_green = true;
                    break;
                }
            }
        }

        if (was_green) set_lights(lane, false, true, false); 
        else set_lights(lane, true, false, false); 
    }
}

// 3. PEDESTRIAN RED
void apply_all_red(Intersection *intr) {
    for (uint32_t i = 0; i < intr->lane_cnt; i++) {
        Lane *lane = &intr->lanes[i];
        if (lane->type != LANE_IN) continue;
        set_lights(lane, true, false, false); // Red ON
    }
}

// --- LOGIC FUNCTIONS ---

void simulate_traffic_changes(Intersection *intr) {
    if (DEBUG) Serial.println("\n--- Sensor Readings ---");

    // 1. Simulate ARRIVALS
    for (uint32_t i = 0; i < intr->lane_cnt; i++) {
        if (intr->lanes[i].type == LANE_IN) {
            int sensor_val = received_sensor_value[i];
            int arrival_probability = map(sensor_val, 0, 1023, 5, 100);

            if (random(0, 100) < arrival_probability) {
                intr->lanes[i].current_traffic_value++;
                if (intr->lanes[i].current_traffic_value > 255)
                    intr->lanes[i].current_traffic_value = 255;
            }
        }
    }

    // 2. Simulate DEPARTURES
    if (current_state == STATE_GREEN_RUNNING) {
        const Phase *curr = &intr->phases[current_phase_idx];
        for (uint32_t c = 0; c < intr->connection_cnt; c++) {
            if (curr->active_connections_mask & (1ULL << c)) {
                uint32_t src = intr->connections[c].source_lane_idx;
                if (intr->lanes[src].current_traffic_value > 0) {
                    if (random(0, 100) < 50) {
                        intr->lanes[src].current_traffic_value--;
                    }
                }
            }
        }
    }
}

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

int determine_next_phase(Intersection *intr) {
    int best_phase_idx = -1;
    unsigned long now = millis();
    unsigned long current_duration = now - current_phase_start_time;

    Serial.println("\n--- Decision Time ---");

    int32_t total_system_pressure = 0;
    for (uint32_t i = 0; i < intr->phase_cnt; i++) {
        total_system_pressure += calculate_phase_pressure(intr, i);
    }

    // IDLE MODE
    if (total_system_pressure == 0) {
        Serial.println(">> No Traffic Detected. Using Timer Logic.");
        uint32_t target_duration = intr->phases[current_phase_idx].duration_ms;
        if (target_duration == 0) target_duration = intr->default_phase_duration_ms;

        if (current_duration >= target_duration) {
            return (current_phase_idx + 1) % intr->phase_cnt;
        } else {
            return current_phase_idx;
        }
    } 
    
    // MAX PRESSURE MODE
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

    if (starved_phase != -1) return starved_phase;

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
        return current_phase_idx;
    }

    return (best_phase_idx == -1) ? current_phase_idx : best_phase_idx;
}

// --- PUBLIC API ---

void controller_setup() {
    Serial.println("--- Initializing Hardware from Config ---");
    initialize_hardware(&intr);

    unsigned long now = millis();
    current_phase_start_time = now;
    last_pedestrian_time = now; // Initialize cooldown
    for (int i = 0; i < 4; i++) phase_last_serviced[i] = now;

    current_state = STATE_GREEN_RUNNING;
    apply_phase_lights_green(&intr, current_phase_idx);
}

void controller_loop() {
    if (intr.lane_cnt == 0) return;

    unsigned long now = millis();

    // --- 1. SIMULATION ---
    if (SIMULATION_MODE) {
        if (now - last_simulation_time > SIMULATE_TRAFFIC_INTERVAL) {
            simulate_traffic_changes(&intr);
            last_simulation_time = now;
        }
    }

    // --- 2. STATE MACHINE ---
    switch (current_state) {
        
        // A: GREEN LIGHTS
        case STATE_GREEN_RUNNING:
            if (now - last_decision_time > DECISION_TIME_INTERVAL) {
                unsigned long current_duration = now - current_phase_start_time;

                if (current_duration > MIN_GREEN_TIME) {
                    int desired_phase = determine_next_phase(&intr);
                    
                    if (desired_phase != current_phase_idx) {
                        Serial.printf(">>> SWITCHING: Phase %d -> Phase %d\n", current_phase_idx, desired_phase);
                        
                        next_pending_phase_idx = desired_phase;
                        current_state = STATE_YELLOW_TRANSITION;
                        transition_start_time = now;
                        
                        // Apply Yellow
                        apply_transition_yellow(&intr, current_phase_idx); 
                        phase_change_counter++;
                    } 
                    else {
                         Serial.printf(">>> EXTENDING: Phase %d (Time: %lu ms)\n", current_phase_idx, current_duration);
                    }
                }
                last_decision_time = now;
            }
            break;

        // B: YELLOW TRANSITION
        case STATE_YELLOW_TRANSITION:
            if (now - transition_start_time > YELLOW_DURATION_MS) {
                
                // PEDESTRIAN CHECK LOGIC
                // 1. Is it changing phases? (Yes, we are in Yellow transition)
                // 2. Has enough time passed since the last pedestrian cycle?
                bool time_for_pedestrians = (now - last_pedestrian_time >= PEDESTRIAN_COOLDOWN_MS);

                if (time_for_pedestrians) {
                    Serial.println(">>> PEDESTRIAN MODE TRIGGERED (ALL RED)");
                    current_state = STATE_PEDESTRIAN_RED;
                    transition_start_time = now;
                    last_pedestrian_time = now; // Reset timer
                    apply_all_red(&intr);
                } 
                else {
                    // Normal Cycle
                    current_state = STATE_GREEN_RUNNING;
                    current_phase_idx = next_pending_phase_idx;
                    current_phase_start_time = now;
                    phase_last_serviced[current_phase_idx] = now;
                    apply_phase_lights_green(&intr, current_phase_idx);
                }
            }
            break;

        // C: PEDESTRIAN ALL RED
        case STATE_PEDESTRIAN_RED:
            if (now - transition_start_time > PEDESTRIAN_DURATION_MS) {
                
                // --- NEW PRINT STATEMENT ---
                Serial.printf(">>> PEDESTRIAN DONE. Resuming Phase %d.\n", next_pending_phase_idx);
                
                // Resume the phase we were supposed to go to
                current_state = STATE_GREEN_RUNNING;
                current_phase_idx = next_pending_phase_idx;
                current_phase_start_time = now;
                phase_last_serviced[current_phase_idx] = now;
                apply_phase_lights_green(&intr, current_phase_idx);
            }
            break;
    }
}