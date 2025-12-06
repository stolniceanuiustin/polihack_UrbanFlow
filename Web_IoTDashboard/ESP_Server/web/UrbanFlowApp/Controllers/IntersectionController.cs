using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Hosting;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using UrbanFlowApp.Models;

namespace UrbanFlowApp.Controllers
{
    public class IntersectionController : Controller
    {
        private readonly IWebHostEnvironment _env;

        private static readonly object _fileLock = new object();

        public IntersectionController(IWebHostEnvironment env)
        {
            _env = env;
        }

        [HttpGet]
        public IActionResult Create()
        {
            return View(new IntersectionViewModel());
        }

        [HttpPost]
        public IActionResult Create(IntersectionViewModel model)
        {

            ModelState.Remove("Id");

            if (model.Connections != null && model.Connections.Count > 64)
            {
                ModelState.AddModelError("", "Eroare: Sistemul suportă maximum 64 de conexiuni (limitare hardware bitmask).");
            }

            if (model.Connections != null && model.Lanes != null)
            {
                for (int i = 0; i < model.Connections.Count; i++)
                {
                    var conn = model.Connections[i];

                    var sourceLane = model.Lanes.FirstOrDefault(l => l.LocalId == conn.SourceLaneIdx);
                    var targetLane = model.Lanes.FirstOrDefault(l => l.LocalId == conn.TargetLaneIdx);

                    if (sourceLane == null || targetLane == null) continue;

                    if (sourceLane.Type != LaneType.LANE_IN)
                    {
                        ModelState.AddModelError($"Connections[{i}].SourceLaneIdx", $"Conexiunea #{i}: Sursa (Banda {conn.SourceLaneIdx}) trebuie să fie de tip IN.");
                    }
                    if (targetLane.Type != LaneType.LANE_OUT)
                    {
                        ModelState.AddModelError($"Connections[{i}].TargetLaneIdx", $"Conexiunea #{i}: Destinația (Banda {conn.TargetLaneIdx}) trebuie să fie de tip OUT.");
                    }
                }
            }

            if (!ModelState.IsValid)
            {
                return View(model);
            }

            var hardwareConfig = new
            {
                id = Guid.NewGuid().ToString(),
                name = model.Name,
                status = "OK",
                default_phase_duration_ms = model.DefaultPhaseDurationMs,

                lane_cnt = model.Lanes.Count,
                lanes = model.Lanes.OrderBy(l => l.LocalId).Select(l => new
                {
                    id = (uint)l.LocalId,
                    type = (int)l.Type,

                    bearing = (l.Bearing % 360 + 360) % 360,

                    current_traffic_value = 0,
                    hw = new
                    {
                        sensor_pin = l.SensorPin,
                        green_pin = l.GreenPin,
                        yellow_pin = l.YellowPin,
                        red_pin = l.RedPin
                    }
                }),

                connection_cnt = model.Connections.Count,
                connections = model.Connections.OrderBy(c => c.Id).Select(c => new
                {
                    source_lane_idx = (uint)c.SourceLaneIdx,
                    target_lane_idx = (uint)c.TargetLaneIdx,
                    weight = (uint)c.Weight
                }),

                phase_cnt = model.Phases.Count,
                phases = model.Phases.Select(p =>
                {
                    ulong mask = 0;

                    if (p.ActiveConnectionIndices != null)
                    {
                        foreach (var connIdx in p.ActiveConnectionIndices)
                        {
                            if (connIdx >= 0 && connIdx < 64)
                            {
                                mask |= (1UL << connIdx);
                            }
                        }
                    }

                    return new
                    {
                        active_connections_mask = mask,
                        duration_ms = (uint)p.DurationMs
                    };
                })
            };

            try
            {
                var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");
                var options = new JsonSerializerOptions { WriteIndented = true };

                lock (_fileLock)
                {
                    List<object> allIntersections;

                    if (System.IO.File.Exists(filePath))
                    {
                        var existingJson = System.IO.File.ReadAllText(filePath);
                        if (string.IsNullOrWhiteSpace(existingJson))
                        {
                            allIntersections = new List<object>();
                        }
                        else
                        {
                            try
                            {
                                allIntersections = JsonSerializer.Deserialize<List<object>>(existingJson) ?? new List<object>();
                            }
                            catch (JsonException)
                            {
                                allIntersections = new List<object>();
                            }
                        }
                    }
                    else
                    {
                        allIntersections = new List<object>();
                    }

                    allIntersections.Add(hardwareConfig);
                    System.IO.File.WriteAllText(filePath, JsonSerializer.Serialize(allIntersections, options));
                }

                return RedirectToAction("Index", "Home");
            }
            catch (Exception ex)
            {
                ModelState.AddModelError("", "Eroare la salvarea fișierului JSON: " + ex.Message);
                return View(model);
            }
        }

        [HttpGet]
        public IActionResult Edit(string id)
        {
            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");
            IntersectionViewModel model = null;

            lock (_fileLock)
            {
                if (System.IO.File.Exists(filePath))
                {
                    var json = System.IO.File.ReadAllText(filePath);
                    using (JsonDocument doc = JsonDocument.Parse(json))
                    {
                        foreach (var element in doc.RootElement.EnumerateArray())
                        {
                            if (element.TryGetProperty("id", out var idProp) && idProp.GetString() == id)
                            {
                                model = new IntersectionViewModel
                                {
                                    Id = id,
                                    Name = element.GetProperty("name").GetString(),
                                    DefaultPhaseDurationMs = element.TryGetProperty("default_phase_duration_ms", out var dur) ? dur.GetInt32() : 3000
                                };
                                break;
                            }
                        }
                    }
                }
            }

            if (model == null) return NotFound();

            return View(model);
        }

        [HttpPost]
        public IActionResult Edit(IntersectionViewModel model)
        {
            if (model.Connections != null && model.Connections.Count > 64)
            {
                ModelState.AddModelError("", "Eroare: Maximum 64 conexiuni.");
            }

            if (model.Connections != null && model.Lanes != null)
            {
                for (int i = 0; i < model.Connections.Count; i++)
                {
                    var conn = model.Connections[i];
                    var sourceLane = model.Lanes.FirstOrDefault(l => l.LocalId == conn.SourceLaneIdx);
                    var targetLane = model.Lanes.FirstOrDefault(l => l.LocalId == conn.TargetLaneIdx);

                    if (sourceLane == null || targetLane == null) continue;

                    if (sourceLane.Type != LaneType.LANE_IN)
                        ModelState.AddModelError($"Connections[{i}].SourceLaneIdx", $"Conexiunea #{i}: Sursa trebuie să fie IN.");
                    if (targetLane.Type != LaneType.LANE_OUT)
                        ModelState.AddModelError($"Connections[{i}].TargetLaneIdx", $"Conexiunea #{i}: Destinația trebuie să fie OUT.");
                }
            }

            if (!ModelState.IsValid) return View(model);

            var hardwareConfig = new
            {
                id = model.Id,
                name = model.Name,
                default_phase_duration_ms = model.DefaultPhaseDurationMs,
                lane_cnt = model.Lanes.Count,
                lanes = model.Lanes.OrderBy(l => l.LocalId).Select(l => new
                {
                    id = (uint)l.LocalId,
                    type = (int)l.Type,

                    bearing = (l.Bearing % 360 + 360) % 360,

                    current_traffic_value = 0,
                    hw = new
                    {
                        sensor_pin = l.SensorPin,
                        green_pin = l.GreenPin,
                        yellow_pin = l.YellowPin,
                        red_pin = l.RedPin
                    }
                }),
                connection_cnt = model.Connections.Count,
                connections = model.Connections.OrderBy(c => c.Id).Select(c => new
                {
                    source_lane_idx = (uint)c.SourceLaneIdx,
                    target_lane_idx = (uint)c.TargetLaneIdx,
                    weight = (uint)c.Weight
                }),
                phase_cnt = model.Phases.Count,
                phases = model.Phases.Select(p =>
                {
                    ulong mask = 0;
                    if (p.ActiveConnectionIndices != null)
                    {
                        foreach (var connIdx in p.ActiveConnectionIndices)
                            if (connIdx >= 0 && connIdx < 64) mask |= (1UL << connIdx);
                    }
                    return new { active_connections_mask = mask, duration_ms = (uint)p.DurationMs };
                })
            };

            try
            {
                var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");
                var options = new JsonSerializerOptions { WriteIndented = true };

                lock (_fileLock)
                {
                    JsonArray jsonArray;

                    if (System.IO.File.Exists(filePath))
                    {
                        var json = System.IO.File.ReadAllText(filePath);
                        var rootNode = JsonNode.Parse(json);
                        jsonArray = rootNode as JsonArray ?? new JsonArray();

                        var nodeToRemove = jsonArray.FirstOrDefault(x => x["id"]?.ToString() == model.Id);
                        if (nodeToRemove != null)
                        {
                            jsonArray.Remove(nodeToRemove);
                        }
                    }
                    else
                    {
                        jsonArray = new JsonArray();
                    }

                    var newNode = JsonSerializer.SerializeToNode(hardwareConfig);
                    jsonArray.Add(newNode);

                    System.IO.File.WriteAllText(filePath, jsonArray.ToJsonString(options));
                }

                return RedirectToAction("Index", "Home");
            }
            catch (Exception ex)
            {
                ModelState.AddModelError("", "Eroare la salvare: " + ex.Message);
                return View(model);
            }
        }

        [HttpPost]
        public IActionResult Delete(string id)
        {
            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

            lock (_fileLock)
            {
                if (System.IO.File.Exists(filePath))
                {
                    var json = System.IO.File.ReadAllText(filePath);

                    var rootNode = JsonNode.Parse(json);

                    if (rootNode is JsonArray jsonArray)
                    {
                        var nodeToRemove = jsonArray.FirstOrDefault(x => x["id"]?.ToString() == id);

                        if (nodeToRemove != null)
                        {
                            jsonArray.Remove(nodeToRemove);

                            var options = new JsonSerializerOptions { WriteIndented = true };
                            System.IO.File.WriteAllText(filePath, rootNode.ToJsonString(options));
                        }
                    }
                }
            }
            return RedirectToAction("Index", "Home");
        }

        [HttpGet]
        public IActionResult GetConfig()
        {
            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

            lock (_fileLock)
            {
                if (!System.IO.File.Exists(filePath))
                {
                    return NotFound(new { error = "Config file not found" });
                }

                try
                {
                    var jsonContent = System.IO.File.ReadAllText(filePath);
                    return Content(jsonContent, "application/json");
                }
                catch
                {
                    return StatusCode(500, new { error = "Could not read config file" });
                }
            }
        }

        [HttpGet]
        public IActionResult GetConfigByName(string name)
        {
            if (string.IsNullOrEmpty(name))
            {
                return BadRequest(new { error = "Trebuie să specificați un nume." });
            }

            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

            lock (_fileLock)
            {
                if (!System.IO.File.Exists(filePath))
                {
                    return NotFound(new { error = "Nu există configurații salvate." });
                }

                try
                {
                    var json = System.IO.File.ReadAllText(filePath);

                    using (JsonDocument doc = JsonDocument.Parse(json))
                    {
                        var root = doc.RootElement;
                        if (root.ValueKind == JsonValueKind.Array)
                        {
                            foreach (var element in root.EnumerateArray())
                            {
                                if (element.TryGetProperty("name", out var nameProp) &&
                                    nameProp.GetString().Equals(name, StringComparison.OrdinalIgnoreCase))
                                {
                                    return Content(element.ToString(), "application/json");
                                }
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    return StatusCode(500, new { error = "Eroare server: " + ex.Message });
                }
            }

            return NotFound(new { error = $"Intersecția cu numele '{name}' nu a fost găsită." });
        }


        [HttpPost] 
        public IActionResult UpdateStatus(string name, string status)
        {
            if (string.IsNullOrEmpty(name) || string.IsNullOrEmpty(status))
                return BadRequest("Nume și Status sunt obligatorii.");

            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

            lock (_fileLock)
            {
                if (System.IO.File.Exists(filePath))
                {
                    try
                    {
                        var json = System.IO.File.ReadAllText(filePath);
                        var rootNode = JsonNode.Parse(json);

                        if (rootNode is JsonArray jsonArray)
                        {
                            var targetNode = jsonArray.FirstOrDefault(x =>
                                x["name"]?.ToString().Equals(name, StringComparison.OrdinalIgnoreCase) == true);

                            if (targetNode != null)
                            {
                                targetNode["status"] = status;

                                var options = new JsonSerializerOptions { WriteIndented = true };
                                System.IO.File.WriteAllText(filePath, jsonArray.ToJsonString(options));

                                return Ok(new { message = "Status actualizat." });
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        return StatusCode(500, ex.Message);
                    }
                }
            }
            return NotFound("Intersecția nu a fost găsită.");
        }

    }
}