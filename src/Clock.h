#ifndef Clock_h
#define Clock_h

#include "BasicESP8266.h"
#include <OneButton.h>
#include <TM1637Display.h>

class Clock {
    public:
        Clock(int dio_pin, int clk_pin, int button_pin, int buzzer_pin);
        void button_tick();

    private:
        void handleClick();
        BasicESP8266 _esp;
        OneButton _button;
        TM1637Display _display;
        int _buzzer_pin;
        int _count;
        int _buzzer_state;
};


#endif