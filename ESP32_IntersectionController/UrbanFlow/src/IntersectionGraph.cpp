#include "IntersectionGraph.h"
#include <string.h> // Required for memset


bool intersection_init(Intersection *intr, uint32_t default_duration_ms) {
    if (!intr) {
        return false;
    }
    memset(intr, 0, sizeof(Intersection));
    
    intr->default_phase_duration_ms = default_duration_ms;
    intr->current_phase_idx = 0;
    
    return true;
}

void intersection_reset(Intersection *intr) {
    if (intr) {
        intr->lane_cnt = 0;
        intr->connection_cnt = 0;
        intr->phase_cnt = 0;
        intr->current_phase_idx = 0;
    }
}

uint32_t add_lane(Intersection *intr, uint32_t id, LaneType type, LaneHardware hw) {
    // 1. Validation
    if (!intr || intr->lane_cnt >= MAX_LANE_CNT) {
        return INTGRAPH_INVALID_INDEX;
    }

    // 2. Check for duplicate ID (Optional safety)
    if (find_lane_index_by_id(intr, id) != INTGRAPH_INVALID_INDEX) {
        return INTGRAPH_INVALID_INDEX; 
    }

    // 3. Assignment
    uint32_t idx = intr->lane_cnt;
    Lane *l = &intr->lanes[idx];

    l->id = id;
    l->type = type;
    l->hw = hw;
    l->current_traffic_value = 0; // Default start value

    intr->lane_cnt++;
    return idx;
}


uint32_t add_connection(Intersection *intr, uint32_t source_lane_idx, uint32_t target_lane_idx) {
    if (!intr || intr->connection_cnt >= MAX_CONNECTION_CNT) {
        return INTGRAPH_INVALID_INDEX;
    }

    if (source_lane_idx >= intr->lane_cnt || target_lane_idx >= intr->lane_cnt) {
        return INTGRAPH_INVALID_INDEX;
    }

    uint32_t idx = intr->connection_cnt;
    Connection *c = &intr->connections[idx];

    c->source_lane_idx = source_lane_idx;
    c->target_lane_idx = target_lane_idx;
    c->weight = 1;

    intr->connection_cnt++;
    return idx;
}

void add_phase(Intersection *intr, uint64_t connection_mask, uint32_t duration_ms) {
    if (!intr || intr->phase_cnt >= MAX_PHASE_CNT) {
        return;
    }

    uint32_t idx = intr->phase_cnt;
    Phase *p = &intr->phases[idx];

    p->active_connections_mask = connection_mask;

    if (duration_ms > 0) {
        p->duration_ms = duration_ms;
    } else {
        p->duration_ms = intr->default_phase_duration_ms;
    }

    intr->phase_cnt++;
}


uint32_t find_lane_index_by_id(Intersection *intr, uint32_t id) {
    if (!intr) {
        return INTGRAPH_INVALID_INDEX;
    }

    for (uint32_t i = 0; i < intr->lane_cnt; i++) {
        if (intr->lanes[i].id == id) {
            return i;
        }
    }
    
    return INTGRAPH_INVALID_INDEX;
}