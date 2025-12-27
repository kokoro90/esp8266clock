#ifndef ESPClock_h
#define ESPClock_h

#include "BasicESP8266.h"
#include <OneButton.h>
#include <TM1637Display.h>

class ESPClock {
    public:
//        ESPClock();
        ESPClock(int dio_pin, int clk_pin, int button_pin, int buzzer_pin);
        void button_tick();

    private:
        BasicESP8266 _esp;
        OneButton _button;
        TM1637Display _display;
        void handleClick();
        int _buzzer_pin;
        int _count;
        int _buzzer_state;
};


#endif