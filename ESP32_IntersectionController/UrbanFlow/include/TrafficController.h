#ifndef TRAFFIC_CONTROLLER_H
#define TRAFFIC_CONTROLLER_H

#include <Arduino.h>
#include "IntersectionGraph.h"

extern Intersection intr;

void controller_setup();
void controller_loop();

#endif