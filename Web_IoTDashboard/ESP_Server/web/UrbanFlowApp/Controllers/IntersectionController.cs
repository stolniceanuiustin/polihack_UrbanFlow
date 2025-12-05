using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Hosting; 
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using UrbanFlowApp.Models; 

namespace UrbanFlowApp.Controllers
{
    public class IntersectionController : Controller
    {
        private readonly IWebHostEnvironment _env;

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
                default_phase_duration_ms = model.DefaultPhaseDurationMs,

                lane_cnt = model.Lanes.Count,
                lanes = model.Lanes.OrderBy(l => l.LocalId).Select(l => new
                {
                    id = (uint)l.LocalId,
                    type = (int)l.Type, 
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
                        allIntersections = JsonSerializer.Deserialize<List<object>>(existingJson);
                    }
                }
                else
                {
                    allIntersections = new List<object>();
                }

                allIntersections.Add(hardwareConfig);

                var options = new JsonSerializerOptions { WriteIndented = true };
                System.IO.File.WriteAllText(filePath, JsonSerializer.Serialize(allIntersections, options));

                return RedirectToAction("Index", "Home"); 
            }
            catch (Exception ex)
            {
                ModelState.AddModelError("", "Eroare la salvarea fișierului JSON: " + ex.Message);
                return View(model);
            }
        }

      
        [HttpGet]
        public IActionResult GetConfig()
        {
            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

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
}