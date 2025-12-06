using System.Diagnostics;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Hosting; 
using System.IO;                    
using System.Collections.Generic;
using System.Text.Json;         
using UrbanFlowApp.Models;

namespace UrbanFlowApp.Controllers
{
    public class HomeController : Controller
    {
        private readonly ILogger<HomeController> _logger;
        private readonly IWebHostEnvironment _env; 

        public HomeController(ILogger<HomeController> logger, IWebHostEnvironment env)
        {
            _logger = logger;
            _env = env;
        }

        public IActionResult Index()
        {
            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

            List<dynamic> intersections = new List<dynamic>();

            if (System.IO.File.Exists(filePath))
            {
                try
                {
                    var json = System.IO.File.ReadAllText(filePath);

                    if (!string.IsNullOrWhiteSpace(json))
                    {
                        intersections = JsonSerializer.Deserialize<List<dynamic>>(json) ?? new List<dynamic>();
                    }
                }
                catch (Exception ex)
                {
                    _logger.LogError("Eroare la citirea intersections.json: " + ex.Message);
                }
            }

            return View(intersections);
        }

        public IActionResult Privacy()
        {
            return View();
        }

        [ResponseCache(Duration = 0, Location = ResponseCacheLocation.None, NoStore = true)]
        public IActionResult Error()
        {
            return View(new ErrorViewModel { RequestId = Activity.Current?.Id ?? HttpContext.TraceIdentifier });
        }
    }
}