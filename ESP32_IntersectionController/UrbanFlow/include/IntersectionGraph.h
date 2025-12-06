#ifndef INTGRAPH_H
#define INTGRAPH_H

#include <stdbool.h>
#include <stdint.h>

// --- Configuration & Constants ---
#define MAX_LANE_CNT 64
#define MAX_CONNECTION_CNT 64 
#define MAX_PHASE_CNT 32

// Error sentinel. Check against this when aitdding lanes/connections.
#define INTGRAPH_INVALID_INDEX 0xFFFFFFFF 

typedef enum {
    LANE_IN, // Traffic entering the intersection
    LANE_OUT   // Traffic leaving the intersection
} LaneType;

typedef struct {
    int16_t sensor_pin; // Use -1 if no sensor
    int16_t green_pin;  
    int16_t yellow_pin;
    int16_t red_pin;
} LaneHardware;

typedef struct {
    uint32_t id;       
    LaneType type;
    uint32_t current_traffic_value; 
    LaneHardware hw; 
    uint16_t bearing; //Entry/exit angle - for conflict calculation 
} Lane;

typedef struct {
    uint32_t source_lane_idx;    
    uint32_t target_lane_idx;    
    uint32_t weight;        
} Connection;

typedef struct {
    // Bit N high means Connection N is GREEN.
    uint64_t active_connections_mask; 
    
    // Duration for this specific phase. 
    // If set to 0 during init, it will inherit the Intersection's default.
    uint32_t duration_ms; 
} Phase;

typedef struct {
    // Storage
    Lane lanes[MAX_LANE_CNT];
    uint32_t lane_cnt;

    Connection connections[MAX_CONNECTION_CNT];
    uint32_t connection_cnt;

    Phase phases[MAX_PHASE_CNT];
    uint32_t phase_cnt;

    // Configuration
    uint32_t default_phase_duration_ms;

   

    // Runtime State
    uint32_t current_phase_idx;

    //Conflict computation on edge device
    uint64_t conflict_masks[MAX_CONNECTION_CNT];



} Intersection;

// --- API ---
bool intersection_init(Intersection *intr, uint32_t default_duration_ms);

void intersection_reset(Intersection *intr);


// Returns INTGRAPH_INVALID_INDEX on failure
uint32_t add_lane(Intersection *intr, uint32_t id, LaneType type, LaneHardware hw, uint16_t bearing);

// Returns INTGRAPH_INVALID_INDEX on failure
uint32_t add_connection(Intersection *intr, uint32_t source_lane_idx, uint32_t target_lane_idx);

// Add a phase. 
// Pass duration_ms = 0 to use the Intersection's default time.
void add_phase(Intersection *intr, uint64_t connection_mask, uint32_t duration_ms);

// --- Helpers ---
uint32_t find_lane_index_by_id(Intersection *intr, uint32_t id);

// --- SAFETY CHECKS ---
void compute_conflicts_on_device(Intersection *intr);
bool is_phase_safe(Intersection *intr, uint64_t phase_mask);

#endif