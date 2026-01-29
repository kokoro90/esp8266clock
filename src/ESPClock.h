#ifndef ESPClock_h
#define ESPClock_h

#include "BasicESP8266.h"
#include <OneButton.h>
#include <TM1637Display.h>

class ESPClock {
    public:
        ESPClock(bool debug, int dio_pin, int clk_pin, int button_pin, int buzzer_pin);
        void button_tick();
        void displayTime();

    private:
        BasicESP8266 _esp;
        OneButton _button;
        TM1637Display _display;
        int _brightness = 3;
        void handleClick();
        void handleLongPress();
        int _buzzer_pin;
        int _count;
        int _buzzer_state;
        uint32_t _lastUpdated = 0;
        uint32_t _previousTime = 0;
        int _showColon = 128;
        bool _blink = false;
        bool _debug;
        uint16_t _alarmTime;
        bool _alarmActive = false;
        bool _alarmOn = false;
        int _button_presses = 0;

        void _setEndPoints();
        char *_getAlarmTimeStr();
        void _handleAlarm();
};


#endif