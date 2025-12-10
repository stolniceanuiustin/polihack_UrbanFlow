# 🚦 UrbanFlow - Smart Traffic Management Dashboard

## Developed by: Popescu Florian

---

## About this Project

This project serves as the Web Interface (Dashboard) and Configuration Manager for the **UrbanFlow Smart Traffic System**. Its primary purpose is to allow users to graphically **configure, deploy, and monitor** signalized intersection settings, generating a standardized JSON configuration file that is consumed by edge hardware devices (e.g., ESP32 microcontrollers).

The Web Application acts as the **Control Plane**, while the hardware device acts as the **Data Plane** that executes the retrieved configuration.

### Key Roles:

* **GUI:** Provides a user-friendly interface to define lanes, routes (Connections), and light sequences (Phases).
* **Data Generation:** Serializes the configuration models into a single, compact JSON file (`intersections.json`).
* **API Endpoint:** Exposes a read-only API endpoint (`/Intersection/GetConfigByName`) for the ESP device to fetch its specific configuration payload. The hardware can also use the `UpdateStatus` endpoint to report its operational health back to the dashboard.

---

## Architecture

The application is built upon the classic **Model-View-Controller (MVC)** architectural pattern using ASP.NET Core, ensuring a clear **Separation of Concerns**.

| Component | Responsibility | Relevant Files |
| :--- | :--- | :--- |
| **Model** | Data structure and business validation (e.g., Lane types) | `IntersectionModels.cs` |
| **View** | User Interface and Data Presentation (Razor syntax) | `Index.cshtml`, `Create.cshtml`, `Edit.cshtml` |
| **Controller** | Handling HTTP requests, business logic, data persistence (JSON I/O), and API exposure. | `HomeController.cs`, `IntersectionController.cs` |

### **Communication Protocol (Web ↔ Hardware)**

The system leverages the JSON configuration file as the standardized data contract between the Web Dashboard and the ESP traffic light controller.

1.  **Data Persistence:** The controller saves all intersection configurations to a central `intersections.json` file on the server. The file access is protected using a static lock (`_fileLock`) to ensure data integrity during concurrent operations.
2.  **Configuration Format:** The core data structure for phases uses a **64-bit Bitmask** (`ulong`) to efficiently represent active connections (GREEN light status) in a specific phase. This is crucial for fast, low-footprint processing on microcontrollers.
    $$\text{Active Connections Mask} = \sum_{i=0}^{N} 2^{\text{Connection Index}_i}$$
3.  **API Access:** The ESP device makes an HTTP GET request to the Dashboard's API (`/Intersection/GetConfigByName`) to receive its specific configuration.

---

## Key Features & Workflow

### 1. Configuration (`Create` / `Edit` Views)

* **Lane Definition:** Define the total number of lanes (max 16) and specify their type (`0` for IN or `1` for OUT).
* **Hardware Pin Mapping:** Assign specific microcontroller pins (Sensor, Red, Yellow, Green) to each lane for physical control.
* **Connection Mapping:** Define the valid routes as connections between `IN` (Source) and `OUT` (Target) lanes (e.g., Lane 0 $\rightarrow$ Lane 3).
* **Phase Configuration:** Define sequential phases, their duration (in milliseconds), and select the active connections (GREEN light) for each phase.

### 2. Dashboard & Monitoring (`Index` View)

* Displays a list of all configured intersections.
* Sorts intersections, prioritizing those with an error status (status $\neq$ "OK").
* Presents operational status visually (Green checkmark for "OK", Red X for error).
* Provides actions to Reconfigure or Delete intersections.