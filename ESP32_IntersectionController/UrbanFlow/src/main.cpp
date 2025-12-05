#include <Arduino.h>
#include "IntersectionGraph.h"


#define DEBUG false
// Global instance
Intersection intr;

// --- TIMING CONSTANTS ---
const uint32_t MIN_GREEN_TIME = 5000;        // Minimum time a light stays green
const uint32_t MAX_GREEN_TIME = 50000;       // Maximum time a light CAN stay green before yielding
const uint32_t STARVATION_THRESHOLD = 99999; // If a lane waits this long, it gets priority (Fairness)
const uint32_t SIMULATE_TRAFFIC_INTERVAL = 100;
const uint32_t DECISION_TIME_INTERVAL = 1000;
// --- STATE VARIABLES ---
unsigned long last_decision_time = 0;       // When we last checked the logic
unsigned long last_simulation_time = 0;     // When we last updated traffic
unsigned long current_phase_start_time = 0; // When the CURRENT phase started
uint32_t current_phase_idx = 0;

// Track when each phase was last active to prevent starvation
unsigned long phase_last_serviced[4] = {0, 0, 0, 0};

// --- Helper: Hardware Control ---

void set_lights(const Lane *lane, bool red, bool yellow, bool green)
{
    if (lane->hw.red_pin != -1)
        digitalWrite(lane->hw.red_pin, red ? HIGH : LOW);
    if (lane->hw.yellow_pin != -1)
        digitalWrite(lane->hw.yellow_pin, yellow ? HIGH : LOW);
    if (lane->hw.green_pin != -1)
        digitalWrite(lane->hw.green_pin, green ? HIGH : LOW);
}

void initialize_hardware(Intersection *intr)
{
    for (uint32_t i = 0; i < intr->lane_cnt; i++)
    {
        const Lane *lane = &intr->lanes[i];

        // Output Pins (Lights)
        if (lane->hw.red_pin != -1)
        {
            pinMode(lane->hw.red_pin, OUTPUT);
            digitalWrite(lane->hw.red_pin, HIGH);
        }
        if (lane->hw.yellow_pin != -1)
        {
            pinMode(lane->hw.yellow_pin, OUTPUT);
            digitalWrite(lane->hw.yellow_pin, LOW);
        }
        if (lane->hw.green_pin != -1)
        {
            pinMode(lane->hw.green_pin, OUTPUT);
            digitalWrite(lane->hw.green_pin, LOW);
        }

        // Input Pins (Sensors)
        if (lane->hw.sensor_pin != -1)
        {
            pinMode(lane->hw.sensor_pin, INPUT);
        }
    }
}

// --- SIMULATION LOGIC ---

void simulate_traffic_changes(Intersection *intr)
{
    if (DEBUG)
        Serial.println("\n--- Sensor Readings ---");

    // 1. ARRIVALS
    for (uint32_t i = 0; i < intr->lane_cnt; i++)
    {
        if (intr->lanes[i].type == LANE_IN && intr->lanes[i].hw.sensor_pin != -1)
        {
            int sensor_val = analogRead(intr->lanes[i].hw.sensor_pin);
            int arrival_probability = map(sensor_val, 0, 4095, 5, 100);

            // Print status
            if (DEBUG)
                Serial.printf("Lane %d (Pin %d): Val=%d (Prob=%d%%) | Queue: %d\n",
                              intr->lanes[i].id, intr->lanes[i].hw.sensor_pin, sensor_val, arrival_probability, intr->lanes[i].current_traffic_value);

            if (random(0, 100) < arrival_probability)
            {
                intr->lanes[i].current_traffic_value++;
                if (intr->lanes[i].current_traffic_value > 255)
                    intr->lanes[i].current_traffic_value = 255;
            }
        }
    }

    // 2. DEPARTURES
    const Phase *curr = &intr->phases[current_phase_idx];
    for (uint32_t c = 0; c < intr->connection_cnt; c++)
    {
        if (curr->active_connections_mask & (1ULL << c))
        {
            uint32_t source_idx = intr->connections[c].source_lane_idx;
            if (intr->lanes[source_idx].current_traffic_value > 0)
            {
                if (random(0, 100) < 50)
                {
                    intr->lanes[source_idx].current_traffic_value--;
                }
            }
        }
    }
}

// --- MAX PRESSURE ALGORITHM + FAIRNESS ---

int32_t calculate_phase_pressure(Intersection *intr, int phase_index)
{
    int32_t pressure = 0;
    const Phase *p = &intr->phases[phase_index];

    for (uint32_t c = 0; c < intr->connection_cnt; c++)
    {
        if (p->active_connections_mask & (1ULL << c))
        {
            uint32_t source_idx = intr->connections[c].source_lane_idx;
            pressure += intr->lanes[source_idx].current_traffic_value;
        }
    }
    return pressure;
}

void apply_phase_lights(Intersection *intr, int phase_idx)
{
    const Phase *next_phase = &intr->phases[phase_idx];

    for (uint32_t i = 0; i < intr->lane_cnt; i++)
    {
        Lane *lane = &intr->lanes[i];
        if (lane->type != LANE_IN)
            continue;

        bool is_green = false;
        for (uint32_t c = 0; c < intr->connection_cnt; c++)
        {
            if (intr->connections[c].source_lane_idx == i)
            {
                if (next_phase->active_connections_mask & (1ULL << c))
                {
                    is_green = true;
                    break;
                }
            }
        }

        if (is_green)
            set_lights(lane, false, false, true);
        else
            set_lights(lane, true, false, false);
    }
}

void run_max_pressure_decision(Intersection *intr)
{
    int best_phase_idx = -1;
    int32_t max_pressure = -1;
    unsigned long now = millis();
    unsigned long current_duration = now - current_phase_start_time;

    Serial.println("\n--- Decision Time ---");

    // --- 1. CHECK FOR STARVATION (Fairness) ---
    // If a phase hasn't been green for STARVATION_THRESHOLD and has traffic, force it.
    int starved_phase = -1;
    unsigned long max_wait_time = 0;

    for (uint32_t i = 0; i < intr->phase_cnt; i++)
    {
        if (i == current_phase_idx)
            continue; // Skip current

        // Calculate queue size for this phase to ensure we don't switch to an empty starved lane
        if (calculate_phase_pressure(intr, i) > 0)
        {
            unsigned long wait_time = now - phase_last_serviced[i];

            if (wait_time > STARVATION_THRESHOLD)
            {
                Serial.printf("!! Phase %d is STARVING (Wait: %lums). Priority Override.\n", i, wait_time);
                if (wait_time > max_wait_time)
                {
                    max_wait_time = wait_time;
                    starved_phase = i;
                }
            }
        }
    }

    if (starved_phase != -1)
    {
        best_phase_idx = starved_phase;
    }
    else
    {
        // --- 2. STANDARD MAX PRESSURE LOGIC ---

        bool force_switch = false;

        // Check Max Green Time Limit
        if (current_duration >= MAX_GREEN_TIME)
        {
            Serial.println(">> MAX GREEN TIME EXCEEDED. Forcing yield if traffic exists elsewhere.");
            force_switch = true;
        }

        for (uint32_t i = 0; i < intr->phase_cnt; i++)
        {
            // If forcing switch, skip the current phase in calculation
            if (force_switch && i == current_phase_idx)
                continue;

            int32_t p = calculate_phase_pressure(intr, i);
            Serial.printf("Phase %d Pressure: %d\n", i, p);

            if (p > max_pressure)
            {
                max_pressure = p;
                best_phase_idx = i;
            }
        }

        // Hysteresis / Extension Logic
        // If we aren't forcing a switch, and current pressure is equal to max, STAY on current.
        if (!force_switch && max_pressure == calculate_phase_pressure(intr, current_phase_idx))
        {
            best_phase_idx = current_phase_idx;
        }
    }

    // --- 3. APPLY DECISION ---

    // Safety check if best_phase still -1 (e.g., 0 traffic everywhere), keep current
    if (best_phase_idx == -1)
        best_phase_idx = current_phase_idx;

    if (best_phase_idx != current_phase_idx)
    {
        Serial.printf(">>> SWITCHING: Phase %d -> Phase %d\n", current_phase_idx, best_phase_idx);

        current_phase_idx = best_phase_idx;
        current_phase_start_time = now;               // Reset timer for new phase
        phase_last_serviced[current_phase_idx] = now; // Mark this phase as serviced

        apply_phase_lights(intr, current_phase_idx);
    }
    else
    {
        Serial.printf(">>> EXTENDING: Phase %d (Duration: %lums)\n", current_phase_idx, current_duration);
    }

    last_decision_time = now;
}

// --- Setup ---

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- Max Pressure + Fairness Controller ---");

    intersection_init(&intr, 5000);

    // --- HARDWARE CONFIGURATION ---
    LaneHardware hw_north = {.sensor_pin = 8, .green_pin = 41, .yellow_pin = 21, .red_pin = 42};
    LaneHardware hw_south = {.sensor_pin = 9, .green_pin = 15, .yellow_pin = 21, .red_pin = 16};
    LaneHardware hw_west = {.sensor_pin = 10, .green_pin = 11, .yellow_pin = 21, .red_pin = 12};
    LaneHardware hw_east = {.sensor_pin = 3, .green_pin = 4, .yellow_pin = 21, .red_pin = 5};
    LaneHardware hw_dummy = {.sensor_pin = -1, .green_pin = -1, .yellow_pin = -1, .red_pin = -1};

    // Add Lanes
    uint32_t l_west_out = add_lane(&intr, 1, LANE_OUT, hw_dummy);
    uint32_t l_west_in = add_lane(&intr, 2, LANE_IN, hw_west);
    uint32_t l_south_out = add_lane(&intr, 3, LANE_OUT, hw_dummy);
    uint32_t l_south_in = add_lane(&intr, 4, LANE_IN, hw_south);
    uint32_t l_east_out = add_lane(&intr, 5, LANE_OUT, hw_dummy);
    uint32_t l_east_in = add_lane(&intr, 6, LANE_IN, hw_east);
    uint32_t l_north_out = add_lane(&intr, 7, LANE_OUT, hw_dummy);
    uint32_t l_north_in = add_lane(&intr, 8, LANE_IN, hw_north);

    // Connections
    uint32_t conn_west_south = add_connection(&intr, l_west_in, l_south_out);
    uint32_t conn_west_east = add_connection(&intr, l_west_in, l_east_out);
    uint32_t conn_west_north = add_connection(&intr, l_west_in, l_north_out);

    uint32_t conn_south_west = add_connection(&intr, l_south_in, l_west_out);
    uint32_t conn_south_east = add_connection(&intr, l_south_in, l_east_out);
    uint32_t conn_south_north = add_connection(&intr, l_south_in, l_north_out);

    uint32_t conn_east_west = add_connection(&intr, l_east_in, l_west_out);
    uint32_t conn_east_south = add_connection(&intr, l_east_in, l_south_out);
    uint32_t conn_east_north = add_connection(&intr, l_east_in, l_north_out);

    uint32_t conn_north_west = add_connection(&intr, l_north_in, l_west_out);
    uint32_t conn_north_south = add_connection(&intr, l_north_in, l_south_out);
    uint32_t conn_north_east = add_connection(&intr, l_north_in, l_east_out);

    //Phase 0 = WEST, PHASE 1 = SOUTH, PHASE 2 = EAST, PHASE 3 = NPRTH 
    // Phases
    add_phase(&intr, (1ULL << conn_west_south | 1ULL << conn_west_east | 1ULL << conn_west_north), 0);
    add_phase(&intr, (1ULL << conn_south_west | 1ULL << conn_south_east | 1ULL << conn_south_north), 0);
    add_phase(&intr, (1ULL << conn_east_west | 1ULL << conn_east_south | 1ULL << conn_east_north), 0);
    add_phase(&intr, (1ULL << conn_north_west | 1ULL << conn_north_south | 1ULL << conn_north_east), 0);

    initialize_hardware(&intr);

    // Initialize Timing
    unsigned long now = millis();
    current_phase_start_time = now;
    for (int i = 0; i < 4; i++)
        phase_last_serviced[i] = now;

    apply_phase_lights(&intr, current_phase_idx);
}

// --- Loop ---

void loop()
{
    unsigned long now = millis();

    // 1. Run Simulation (Sensor Read -> Probability -> Queue Update)
    if (now - last_simulation_time > SIMULATE_TRAFFIC_INTERVAL)
    {
        simulate_traffic_changes(&intr);
        last_simulation_time = now;
    }

    // 2. Run Decision Logic
    // We check periodically (e.g., every 1s) if MIN_GREEN_TIME has passed.
    // Checking frequently allows us to detect Starvation instantly after Min Green expires.
    if (now - last_decision_time > DECISION_TIME_INTERVAL)
    {
        // Only allow changes if MIN_GREEN_TIME has passed
        if (now - current_phase_start_time > MIN_GREEN_TIME)
        {
            run_max_pressure_decision(&intr);
        }
    }
}
