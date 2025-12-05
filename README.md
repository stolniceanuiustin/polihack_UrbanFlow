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

## Arhitecture 
Our system is based on controllers for each individual intersection. All the controllers are connected to a server via WiFi. The server's role is to send the intersection configuration, which is gathered by the server, from the administrator. The server exposes an endpoint which the ESP will access for the intersection configuration.  

## How intersections are stored?  
We use a graph to store the intersecton. Each node represents a lane and contains info about it.

<!-- ROADMAP -->
## Roadmap

- [x] Sensor EMF
  - [ ] Sample size sweet spot
  - [ ] Better Faraday cage
  - [ ] "Deploy" the code to all arduino UNO
  - [ ] Arduino - ESP communication
- [x] ESP
  - [ ] More algorithms
- [x] Web page
  - [ ] Send data from webpage to ESP
- [ ] Scale model
  - [ ] Buy the parts
    - [ ] Plexiglass base
    - [ ] Decorations ?
  - [ ] Car simulations
