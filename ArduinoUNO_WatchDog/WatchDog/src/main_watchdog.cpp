// // --- ARDUINO UNO WATCHDOG FIRMWARE ---
// // Role: Listen for a pulse. If it stops, reset the other board.
// #include <Arduino.h>

// #define LISTEN_PIN 2   // Connect to ESP32 Heartbeat Pin
// #define RESET_PIN  3   // Connect to ESP32 EN/RST Pin
// #define STATUS_LED 13  

// //WE NEVER SEND 5V TO RESET, we only pull it down
// void killTarget();

// unsigned long lastPulseTime = 0;
// int lastState = LOW;
// const int TIMEOUT_MS = 2000; // 2 seconds silence = DEATH

// void setup() {
//   Serial.begin(115200);
//   Serial.println("--- WATCHDOG ARMED ---");

//   //We never send 5V to the ESP
//   pinMode(LISTEN_PIN, INPUT);
//   pinMode(RESET_PIN, INPUT); 
  
//   pinMode(STATUS_LED, OUTPUT);
//   lastPulseTime = millis();
// }

// void loop() {
//   int currentState = digitalRead(LISTEN_PIN);
//   if (currentState != lastState) {
//     lastPulseTime = millis(); 
//     lastState = currentState;

//     digitalWrite(STATUS_LED, currentState); 
//   }

//   if (millis() - lastPulseTime > TIMEOUT_MS) {
//     killTarget();
//   }
// }

// void killTarget() {
//   digitalWrite(STATUS_LED, HIGH);

//   pinMode(RESET_PIN, OUTPUT);
  
//   digitalWrite(RESET_PIN, LOW); 
  
//   delay(500);
  

//   pinMode(RESET_PIN, INPUT);
  
//   Serial.println("Target Reset. Waiting for reboot...");
  
//   for(int i=0; i<6; i++) {
//     digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
//     delay(500);
//   }
  
//   // Reset our timer so we don't kill it immediately again
//   lastPulseTime = millis();
//   lastState = digitalRead(LISTEN_PIN);
//   Serial.println("Watchdog Re-Armed.");
// }