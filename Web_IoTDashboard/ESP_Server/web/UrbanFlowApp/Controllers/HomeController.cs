using System.Diagnostics;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Hosting; // Necesar pentru calea fisierului
using System.IO;                    // Necesar pentru operatii cu fisiere
using System.Collections.Generic;
using System.Text.Json;           // Necesar pentru citire JSON
using UrbanFlowApp.Models;

namespace UrbanFlowApp.Controllers
{
    public class HomeController : Controller
    {
        private readonly ILogger<HomeController> _logger;
        private readonly IWebHostEnvironment _env; // Injectam mediul pentru a gasi calea fisierului

        public HomeController(ILogger<HomeController> logger, IWebHostEnvironment env)
        {
            _logger = logger;
            _env = env;
        }

        public IActionResult Index()
        {
            // 1. Definim calea catre fisierul JSON
            var filePath = Path.Combine(_env.ContentRootPath, "intersections.json");

            // Lista care va tine datele citite (folosim dynamic pentru flexibilitate la citire)
            List<dynamic> intersections = new List<dynamic>();

            // 2. Verificam daca fisierul exista
            if (System.IO.File.Exists(filePath))
            {
                try
                {
                    // 3. Citim continutul
                    var json = System.IO.File.ReadAllText(filePath);

                    if (!string.IsNullOrWhiteSpace(json))
                    {
                        // 4. Deserializam JSON-ul in lista
                        intersections = JsonSerializer.Deserialize<List<dynamic>>(json) ?? new List<dynamic>();
                    }
                }
                catch (Exception ex)
                {
                    // Daca fisierul e corupt sau blocat, logam eroarea (optional)
                    _logger.LogError("Eroare la citirea intersections.json: " + ex.Message);
                }
            }

            // 5. Trimitem lista catre View-ul Index
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