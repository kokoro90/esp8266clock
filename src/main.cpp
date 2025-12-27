/*
 * Blink
 * Turns on an LED on for one second,
 * then off for one second, repeatedly.
 */

#include "Arduino.h"
//#include "BasicESP8266.h"
//#include <OneButton.h>
//#include <TM1637Display.h>
#include "Clock.h"

//const int buttonPin = 4;     // the number of the pushbutton pin
//const int ledPin =  5;       // the number of the LED pin

//OneButton button(buttonPin, false, false);
//TM1637Display display(D4, D3);

//const bool debug=true;
//const int sigLed=2;                   // for signalling; -1 if not used
//const bool gLowAct=true;               // Signal led low active
//const bool setAccesspointPwd=false;    // Pwd not required when in AP mode (192.168.4.1)
//const bool showWifiPassword=false;     // make Wifi passwd (in)visible

//BasicESP8266 bas(debug,sigLed,gLowAct,setAccesspointPwd,showWifiPassword);

Clock clock(D3, D4, 4, 5);
// variable for storing the pushbutton status
//int buttonState = 0;
//int ledstate = 0;
//int count = 0;

/*void singleClick();
void doubleClick();
void longPress();
void multiClick();
*/
void setup() {
/*  bas.begin();
  Serial.begin(115200);
  Serial.println("Welcome to ESP8266");
  display.setBrightness(7);
  display.clear();
  button.attachClick(singleClick);
  button.attachDoubleClick(doubleClick);
  button.attachLongPressStop(longPress);
  button.attachMultiClick(multiClick);
  pinMode(ledPin, OUTPUT);
  display.showNumberDec(count);*/
}

void loop() {
  // read the state of the pushbutton value
  //buttonState = digitalRead(buttonPin);
  // check if the pushbutton is pressed.
  // if it is, the buttonState is HIGH
  clock.button_tick();
}
/*
void singleClick() {
  Serial.println("Button clicked");
  if(ledstate == 0) {
    Serial.println("Turning LED on");
    digitalWrite(ledPin, HIGH);
    ledstate = 1;
  } else {
    Serial.println("Turning LED off");
    digitalWrite(ledPin, LOW);
    ledstate = 0;
  }

  display.showNumberDec(++count);
}

void doubleClick() {
  Serial.println("Button double-clicked");
  display.showNumberDec(--count);
}

void longPress() {
  Serial.println("Long press detected");
  count += 5;
  display.showNumberDec(count);
}

void multiClick() {
  int n = button.getNumberClicks();
  Serial.print("Multi-click detected.  Button was clicked ");
  Serial.print(n);
  Serial.println(" times");
  count = 0;
  display.showNumberDec(count);
}
  */