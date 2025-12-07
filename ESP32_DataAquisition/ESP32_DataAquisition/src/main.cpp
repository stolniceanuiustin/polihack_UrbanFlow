#include <Arduino.h>

// Lane analog pin assignments
#define POT_LANE_1 34  
#define POT_LANE_2 35 
#define POT_LANE_4 26
#define POT_LANE_6 33
#define POT_LANE_7 25
#define POT_LANE_9 27


#define TXD2 17   
#define RXD2 16

struct LaneConfig {
  int laneID;      
  int analogPin;   
};

const LaneConfig lanes[] = {
  {1, POT_LANE_1}, 
  {2, POT_LANE_2}, 
  {4, POT_LANE_4}, 
  {6, POT_LANE_6}, 
  {7, POT_LANE_7}, 
  {9, POT_LANE_9}  
};

const int NUM_LANES = sizeof(lanes) / sizeof(lanes[0]);

void setup()
{
    Serial.begin(9600);                         
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); 

    analogReadResolution(10);

    for (int i = 0; i < NUM_LANES; i++) {
        pinMode(lanes[i].analogPin, INPUT);
    }

    Serial.println("Sender READY");
}

void loop()
{
    String outputString = "";
    
    for (int i = 0; i < NUM_LANES; i++) {
        int rawValue = analogRead(lanes[i].analogPin); 
        
        outputString += String(lanes[i].laneID);
        outputString += ",";
        outputString += String(rawValue);
        outputString += " ";
    }

    // Send to RECEIVER
    Serial2.println(outputString);

    // Print for debugging
    Serial.print("[TX] ");
    Serial.println(outputString);

    delay(100);
}
