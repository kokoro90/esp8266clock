#ifndef ESPClock_h
#define ESPClock_h

#include "BasicESP8266.h"
#include <OneButton.h>
#include <TM1637Display.h>

class ESPClock {
    public:
        ESPClock(int dio_pin, int clk_pin, int button_pin, int buzzer_pin);
        void button_tick();
        void displayTime();

    private:
        BasicESP8266 _esp;
        OneButton _button;
        TM1637Display _display;
        void handleClick();
        int _buzzer_pin;
        int _count;
        int _buzzer_state;
        uint32_t _lastUpdated = 0;
        int _showColon = 0x40;

        void _setEndPoints();
};


#endif