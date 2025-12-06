#include "IntersectionGraph.h"
#include <string.h> // Required for memset
#include <vector>
#include <algorithm>
#include <Arduino.h>

typedef struct
{
    int id;
    int angle;
} Point;

bool comparePoints(Point a, Point b)
{
    return a.angle < b.angle;
}
// Helper to manually add a conflict to the matrix
void add_conflict(Intersection *intr, uint32_t conn_idx_a, uint32_t conn_idx_b)
{
    if (!intr || conn_idx_a >= MAX_CONNECTION_CNT || conn_idx_b >= MAX_CONNECTION_CNT)
        return;

    // Set bit B in A's mask
    intr->conflict_masks[conn_idx_a] |= ((uint64_t)1 << conn_idx_b);
    // Set bit A in B's mask (Symmetry)
    intr->conflict_masks[conn_idx_b] |= ((uint64_t)1 << conn_idx_a);
}

bool do_paths_cross(Lane *srcA, Lane *tgtA, Lane *srcB, Lane *tgtB)
{
    if (srcA->id == srcB->id) // if both start from the same point then they diverge(SAFE)
        return false;

    // This line controlls merges. We shall allow merges for this demo
    if (tgtA->id == tgtB->id)
        return false;

    std::vector<Point> points;
    points.push_back({0, srcA->bearing});
    points.push_back({0, tgtA->bearing});
    points.push_back({1, srcB->bearing});
    points.push_back({1, tgtB->bearing});
    std::sort(points.begin(), points.end(), comparePoints);

    // 5. Check the pattern
    // If crossed: 0 -> 1 -> 0 -> 1  (ABAB)
    // If safe:    0 -> 0 -> 1 -> 1  (AABB)

    // We just check if the ID of the first point matches the ID of the second point.
    // If points[0].id == points[1].id, then it is AA... (Safe)
    // If points[0].id != points[1].id, then it is AB... (Potential Cross)

    bool is_abab = (points[0].id != points[1].id) && (points[1].id != points[2].id);

    return is_abab;
}

void compute_conflicts_on_device(Intersection *intr)
{
    if (!intr)
        return;

    Serial.println("[GEO] Computing geometry conflicts...");

    // 1. Clear old conflicts
    memset(intr->conflict_masks, 0, sizeof(intr->conflict_masks));

    // 2. Check every connection against every other connection
    for (int i = 0; i < intr->connection_cnt; i++)
    {
        for (int j = i + 1; j < intr->connection_cnt; j++)
        {

            Connection *cA = &intr->connections[i];
            Connection *cB = &intr->connections[j];

            Lane *srcA = &intr->lanes[cA->source_lane_idx];
            Lane *tgtA = &intr->lanes[cA->target_lane_idx];

            Lane *srcB = &intr->lanes[cB->source_lane_idx];
            Lane *tgtB = &intr->lanes[cB->target_lane_idx];

            if (do_paths_cross(srcA, tgtA, srcB, tgtB))
            {
                add_conflict(intr, i, j);
                Serial.printf("[GEO] CONFLICT: Conn %d vs %d\n", i, j);
            }
        }
    }
}

// The Runtime Safety Check
bool is_phase_safe(Intersection *intr, uint64_t phase_mask)
{
    for (int i = 0; i < intr->connection_cnt; i++)
    {
        // If connection 'i' is active...
        if ((phase_mask >> i) & 1)
        {
            // Check if any of its enemies are also active
            if (phase_mask & intr->conflict_masks[i])
            {
                return false; // UNSAFE
            }
        }
    }
    return true;
}

bool intersection_init(Intersection *intr, uint32_t default_duration_ms)
{
    if (!intr)
    {
        return false;
    }
    memset(intr, 0, sizeof(Intersection));

    intr->default_phase_duration_ms = default_duration_ms;
    intr->current_phase_idx = 0;

    return true;
}

void intersection_reset(Intersection *intr)
{
    if (intr)
    {
        intr->lane_cnt = 0;
        intr->connection_cnt = 0;
        intr->phase_cnt = 0;
        intr->current_phase_idx = 0;
        memset(intr->conflict_masks, 0, sizeof(intr->conflict_masks));
    }
}

uint32_t add_lane(Intersection *intr, uint32_t id, LaneType type, LaneHardware hw, uint16_t bearing)
{
    // 1. Validation
    if (!intr || intr->lane_cnt >= MAX_LANE_CNT)
    {
        return INTGRAPH_INVALID_INDEX;
    }

    // 2. Check for duplicate ID (Optional safety)
    if (find_lane_index_by_id(intr, id) != INTGRAPH_INVALID_INDEX)
    {
        return INTGRAPH_INVALID_INDEX;
    }

    // 3. Assignment
    uint32_t idx = intr->lane_cnt;
    Lane *l = &intr->lanes[idx];

    l->id = id;
    l->type = type;
    l->hw = hw;
    l->current_traffic_value = 0; // Default start value
    l->bearing = bearing;
    intr->lane_cnt++;
    return idx;
}

uint32_t add_connection(Intersection *intr, uint32_t source_lane_idx, uint32_t target_lane_idx)
{
    if (!intr || intr->connection_cnt >= MAX_CONNECTION_CNT)
    {
        return INTGRAPH_INVALID_INDEX;
    }

    if (source_lane_idx >= intr->lane_cnt || target_lane_idx >= intr->lane_cnt)
    {
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

void add_phase(Intersection *intr, uint64_t connection_mask, uint32_t duration_ms)
{
    if (!intr || intr->phase_cnt >= MAX_PHASE_CNT)
    {
        return;
    }

    uint32_t idx = intr->phase_cnt;
    Phase *p = &intr->phases[idx];

    p->active_connections_mask = connection_mask;

    if (duration_ms > 0)
    {
        p->duration_ms = duration_ms;
    }
    else
    {
        p->duration_ms = intr->default_phase_duration_ms;
    }

    intr->phase_cnt++;
}

uint32_t find_lane_index_by_id(Intersection *intr, uint32_t id)
{
    if (!intr)
    {
        return INTGRAPH_INVALID_INDEX;
    }

    for (uint32_t i = 0; i < intr->lane_cnt; i++)
    {
        if (intr->lanes[i].id == id)
        {
            return i;
        }
    }

    return INTGRAPH_INVALID_INDEX;
}