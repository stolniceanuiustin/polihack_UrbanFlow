#include <stdio.h>
#include "IntersectionGraph.h"

int main() {
    Intersection intr;
    
    // 1. Initialize: Set a default phase duration of 5 seconds (5000ms)
    if (!intersection_init(&intr, 5000)) {
        printf("Failed to initialize intersection.\n");
        return -1;
    }

    // 2. Define Hardware: 
    // Real hardware for ingress lanes (sensors + lights), dummy hardware for egress (exit) lanes.
    LaneHardware hw_north = { .sensor_pin = 10, .green_pin = 4 , .yellow_pin = 3, .red_pin = 2};
    LaneHardware hw_east  = { .sensor_pin = 11, .green_pin = 7,  .yellow_pin = 6, .red_pin = 5};
    LaneHardware hw_dummy = { .sensor_pin = -1, .green_pin = -1, .yellow_pin = -1, .red_pin = -1};
    // 3. Build Topology
    // Scenario: Two 1-way streets crossing each other.
    
    // -- Add Lanes --
    // Lane 1: North Input (ID 101)
    uint32_t l_north_in = add_lane(&intr, 101, LANE_IN, hw_north);
    // Lane 2: South Output (ID 102) - Where the North car goes
    uint32_t l_south_out = add_lane(&intr, 102, LANE_OUT, hw_dummy);
    
    // Lane 3: East Input (ID 201)
    uint32_t l_east_in = add_lane(&intr, 201, LANE_IN, hw_east);
    // Lane 4: West Output (ID 202) - Where the East car goes
    uint32_t l_west_out = add_lane(&intr, 202, LANE_OUT, hw_dummy);

    // -- Add Connections (Paths) --
    // Path 1: Car goes North to South
    uint32_t conn_north_south = add_connection(&intr, l_north_in, l_south_out);
    
    // Path 2: Car goes East to West
    uint32_t conn_east_west = add_connection(&intr, l_east_in, l_west_out);

    // 4. Define Logic (Phases)
    
    // Phase 0: North-South Green
    // We pass '0' as duration to use the default 5000ms set in init.
    // Mask: Set the bit corresponding to the North-South connection.
    add_phase(&intr, (1ULL << conn_north_south), 0);

    // Phase 1: East-West Green
    // We want this road to be greener for longer (e.g., 8000ms), overriding the default.
    // Mask: Set the bit corresponding to the East-West connection.
    add_phase(&intr, (1ULL << conn_east_west), 8000);

    // 5. Verification Output
    printf("Intersection Configured Successfully!\n");
    printf("Lanes: %d (Max: %d)\n", intr.lane_cnt, MAX_LANE_CNT);
    printf("Connections: %d\n", intr.connection_cnt);
    printf("Phases: %d\n", intr.phase_cnt);
    
    // Example of checking Phase 1 duration
    printf("Phase 1 Duration: %d ms\n", intr.phases[1].duration_ms);

    return 0;
}