#include "Arduino.h"
#include "ESPClock.h"

#define DIO_PIN 2
#define CLK_PIN 0
#define BUTTON_PIN 3
#define BUZZER_PIN 1


ESPClock *espclock;
long previousMillis = 0;
const long interval = 1000;

void setup() {
  espclock = new ESPClock(false, DIO_PIN, CLK_PIN, BUTTON_PIN, BUZZER_PIN);
}

void loop() {
  espclock->button_tick();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    espclock->displayTime();
  }
}
