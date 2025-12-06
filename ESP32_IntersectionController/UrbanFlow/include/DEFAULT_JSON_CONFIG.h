#ifndef JSON_HG
#define JSON_HG

const char* DEFAULT_OFFLINE_CONFIG = R"json(
{
  "id": "f6d11d07-8118-46df-8982-dd25f55057b1",
  "name": "IntersectieComplexa2",
  "status": "OK",
  "default_phase_duration_ms": 3000,
  "lane_cnt": 10,
  "lanes": [
    {
      "id": 0,
      "type": 1,
      "bearing": 0,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 }
    },
    {
      "id": 1,
      "type": 0,
      "bearing": 0,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": 1, "green_pin": 42, "yellow_pin": 21, "red_pin": 41 }
    },
    {
      "id": 2,
      "type": 0,
      "bearing": 0,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": 2, "green_pin": 40, "yellow_pin": 21, "red_pin": 39 }
    },
    {
      "id": 3,
      "type": 1,
      "bearing": 270,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 }
    },
    {
      "id": 4,
      "type": 0,
      "bearing": 270,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": 4, "green_pin": 38, "yellow_pin": 21, "red_pin": 37 }
    },
    {
      "id": 5,
      "type": 1,
      "bearing": 180,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 }
    },
    {
      "id": 6,
      "type": 0,
      "bearing": 180,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": 6, "green_pin": 8, "yellow_pin": 21, "red_pin": 15 }
    },
    {
      "id": 7,
      "type": 0,
      "bearing": 180,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": 7, "green_pin": 45, "yellow_pin": 21, "red_pin": 48 }
    },
    {
      "id": 8,
      "type": 1,
      "bearing": 90,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": -1, "green_pin": -1, "yellow_pin": -1, "red_pin": -1 }
    },
    {
      "id": 9,
      "type": 0,
      "bearing": 90,
      "current_traffic_value": 0,
      "hw": { "sensor_pin": 9, "green_pin": 12, "yellow_pin": 21, "red_pin": 13 }
    }
  ],
  "connection_cnt": 12,
  "connections": [
    { "source_lane_idx": 1, "target_lane_idx": 8, "weight": 1 },
    { "source_lane_idx": 6, "target_lane_idx": 3, "weight": 1 },
    { "source_lane_idx": 7, "target_lane_idx": 8, "weight": 1 },
    { "source_lane_idx": 7, "target_lane_idx": 0, "weight": 1 },
    { "source_lane_idx": 2, "target_lane_idx": 5, "weight": 1 },
    { "source_lane_idx": 2, "target_lane_idx": 3, "weight": 1 },
    { "source_lane_idx": 9, "target_lane_idx": 0, "weight": 1 },
    { "source_lane_idx": 9, "target_lane_idx": 3, "weight": 1 },
    { "source_lane_idx": 9, "target_lane_idx": 5, "weight": 1 },
    { "source_lane_idx": 4, "target_lane_idx": 0, "weight": 1 },
    { "source_lane_idx": 4, "target_lane_idx": 5, "weight": 1 },
    { "source_lane_idx": 4, "target_lane_idx": 8, "weight": 1 }
  ],
  "phase_cnt": 4,
  "phases": [
    { "active_connections_mask": 3, "duration_ms": 5000 },
    { "active_connections_mask": 60, "duration_ms": 10000 },
    { "active_connections_mask": 448, "duration_ms": 5000 },
    { "active_connections_mask": 3584, "duration_ms": 5000 }
  ]
}
)json";

#endif