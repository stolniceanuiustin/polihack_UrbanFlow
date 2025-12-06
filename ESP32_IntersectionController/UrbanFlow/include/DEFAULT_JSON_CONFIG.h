#ifndef JSON_HG
#define JSON_HG

const char* DEFAULT_OFFLINE_CONFIG = R"json(
{
  "id": "0d6b7a8c-5a37-4471-b565-8fc0a8f4e625",
  "name": "IntersectieIn4",
  "default_phase_duration_ms": 3000,
  "lane_cnt": 8,
  "lanes": [
    { "id": 0, "type": 0, "bearing": 0, "current_traffic_value": 0, "hw": { "sensor_pin": 8, "green_pin": 42, "yellow_pin": 21, "red_pin": 41 } },
    { "id": 1, "type": 1, "bearing": 270, "current_traffic_value": 0, "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 } },
    { "id": 2, "type": 0, "bearing": 270, "current_traffic_value": 0, "hw": { "sensor_pin": 10, "green_pin": 12, "yellow_pin": 21, "red_pin": 11 } },
    { "id": 3, "type": 1, "bearing": 180, "current_traffic_value": 0, "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 } },
    { "id": 4, "type": 0, "bearing": 180, "current_traffic_value": 0, "hw": { "sensor_pin": 9, "green_pin": 16, "yellow_pin": 21, "red_pin": 15 } },
    { "id": 5, "type": 1, "bearing": 90, "current_traffic_value": 0, "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 } },
    { "id": 6, "type": 0, "bearing": 90, "current_traffic_value": 0, "hw": { "sensor_pin": 3, "green_pin": 5, "yellow_pin": 21, "red_pin": 4 } },
    { "id": 7, "type": 1, "bearing": 0, "current_traffic_value": 0, "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 } }
  ],
  "connection_cnt": 12,
  "connections": [
    { "source_lane_idx": 0, "target_lane_idx": 1, "weight": 1 },
    { "source_lane_idx": 0, "target_lane_idx": 3, "weight": 1 },
    { "source_lane_idx": 0, "target_lane_idx": 5, "weight": 1 },
    { "source_lane_idx": 2, "target_lane_idx": 3, "weight": 1 },
    { "source_lane_idx": 2, "target_lane_idx": 5, "weight": 1 },
    { "source_lane_idx": 2, "target_lane_idx": 7, "weight": 1 },
    { "source_lane_idx": 4, "target_lane_idx": 1, "weight": 1 },
    { "source_lane_idx": 4, "target_lane_idx": 5, "weight": 1 },
    { "source_lane_idx": 4, "target_lane_idx": 7, "weight": 1 },
    { "source_lane_idx": 6, "target_lane_idx": 1, "weight": 1 },
    { "source_lane_idx": 6, "target_lane_idx": 3, "weight": 1 },
    { "source_lane_idx": 6, "target_lane_idx": 7, "weight": 1 }
  ],
  "phase_cnt": 4,
  "phases": [
    { "active_connections_mask": 7, "duration_ms": 5000 },
    { "active_connections_mask": 56, "duration_ms": 5000 },
    { "active_connections_mask": 448, "duration_ms": 5000 },
    { "active_connections_mask": 3584, "duration_ms": 5000 }
  ]
}
)json";

#endif