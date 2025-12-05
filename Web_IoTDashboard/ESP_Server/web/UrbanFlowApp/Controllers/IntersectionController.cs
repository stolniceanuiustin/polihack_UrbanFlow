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

        // Obiect static pentru a bloca accesul simultan la fisier (Thread Safety)
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
            // 1. Validare: Numarul maxim de conexiuni (pentru bitmask 64-bit)
            if (model.Connections != null && model.Connections.Count > 64)
            {
                ModelState.AddModelError("", "Eroare: Sistemul suportă maximum 64 de conexiuni (limitare hardware bitmask).");
            }

            // 2. Validare: Tipuri de benzi (IN/OUT)
            if (model.Connections != null && model.Lanes != null)
            {
                for (int i = 0; i < model.Connections.Count; i++)
                {
                    var conn = model.Connections[i];

                    // Folosim LocalId pentru a gasi banda corecta, nu indexul din lista
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

            // 3. Constructia obiectului Hardware (JSON Schema)
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
                            // Safety check suplimentar
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

            // 4. Salvarea in fisier cu LOCK
            try
            {
                var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");
                var options = new JsonSerializerOptions { WriteIndented = true };

                // Blocam executia aici pentru ca un singur thread sa scrie in fisier la un moment dat
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
                                // Daca fisierul e corupt, incepem unul nou (sau logam eroarea)
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
        public IActionResult GetConfig()
        {
            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

            // Folosim lock si la citire pentru a nu citi in timp ce altcineva scrie (partial write)
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
    }
}