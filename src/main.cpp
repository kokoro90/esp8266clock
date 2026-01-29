#include "Arduino.h"
#include "ESPClock.h"

#define DIO_PIN 2
#define CLK_PIN 0
#define BUTTON_PIN 4
#define BUZZER_PIN 5


ESPClock *espclock;
long previousMillis = 0;
const long interval = 500;

void setup() {
  espclock = new ESPClock(true, DIO_PIN, CLK_PIN, BUTTON_PIN, BUZZER_PIN);
}

void loop() {
  espclock->button_tick();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    espclock->displayTime();
  }
}
