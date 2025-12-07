## UrbanFlow - UpSmart existing traffic lights in no time!
UrbanFlow is a product prototype developed at Polihack v.18, by team Camera521, composed of: 
   *   Stolniceanu Iustin-Pavel,
   *   Tuchila Adi-Bogdan,
   *   Popescu Florian,
   *   Chira Matei
   *   Todi Tinu-Constantin  

         
This prototype allows municipalities to retrofit existing traffic lights, enabling smart controll at existing intersections with very low cost compared to other solutions. We offer multiple traffic control algorithms:   
   * Fixed-time : Just to showcase the difference between the old and new configuration    
   * Max-Pressure with Phase-Switching Loss Consideration  
   * Self Organising Traffic Lights
   * Lowest Queue First
All of these algorithms are heavily studied and proven in multiple cities around the world and in countless simulation studies.

## Embedded System Arhitecture - The main traffic control unit 
Our embedded prototype is ESP32S3 based, with each intersection controlled by a unique ESP, independently. It has two parts: The traffic control unit and the data aquisition unit.  
# Trafifc Control Unit Tehnical Overview  
  * Lane representation: each lane in the intersection is stored as a node in a graph, containing:
      * Lane ID
      * Sensor pin for vehicle detection
      * Green, yellow, red output pins (to relays IRL, to LEDs in our demo)
  * Lane connections (allowed movements - turns)
  * Phase management - minimum phase time and a mask of active connections, with integraded safety checks  
# Trafic Algorithm  
  * Max Pressure Algoritm decides next phase - time is dinamically allocated based on traffic sensor values

## Embedded Features 
  * Serial Communication between the Data Aquisition Unit and the Traffic Control Unit
  * WiFi Connectivity : Intersection layout is dinamically fetched from a web server where you can model any intersection you want
  * Simulation Mode - allows testing without real sensors
  * Safety Checks - built-in, runtime validation -> ensures unsafe phase combinations are blocked

## How it works 
  * You design the intersection in the dashboard
  * You turn on your ESP32. It connects to WiFI and fetches intersection configuration from the server
  * An intersection graph is initialized based on JSON configuration.
  * Phase Decision -> Traffic control algorithms decide the next phase
  * Light Control -> drives the selected pins in high or low based on phase

## Conclusion 
  * UrbanFlow makes any existing traffic light "smart", providing a low-cost, scalable, drop in solution for cities seeking to optimize traffic flow while maintaining safety.
<!-- ROADMAP -->
