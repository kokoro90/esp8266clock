#include "Arduino.h"
#include "ESPClock.h"

ESPClock *espclock;
long previousMillis = 0;
const long interval = 1000;

void setup() {
  espclock = new ESPClock(0, 2, 4, 5);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    espclock->displayTime();
  }

  espclock->button_tick();
}
