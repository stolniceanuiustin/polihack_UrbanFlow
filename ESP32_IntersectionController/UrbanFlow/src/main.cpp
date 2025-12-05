#include <Arduino.h>
#include "TrafficController.h"

void setup()
{
    Serial.begin(115200);
    delay(2000);
    controller_setup();
}

void loop()
{
    controller_loop();
}