#ifndef ESPClock_h
#define ESPClock_h

#include "BasicESP8266.h"
#include <OneButton.h>
#include <TM1637Display.h>
#include <ArduinoJson.h>

class ESPClock {
    public:
        ESPClock(bool debug, int dio_pin, int clk_pin, int button_pin, int buzzer_pin);
        void button_tick();
        void loop();
        void doDisplay();

    private:
        BasicESP8266 _esp;
        OneButton _button;
        TM1637Display _display;
        JsonDocument _clockConfig;
        enum _state { CLOCK, ALARMTIME, ON, OFF, TIMER };
        enum _state _displayState;
        uint16_t _displayDuration = 3000;
        uint32_t _displayStartTime = 0;
        int _brightness = 3;
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
        bool _twelveHours = false;

        void _displayTime();
        void _displayAlarmTime();
        void _displayString(String str);
        void _setEndPoints();
        void _handleAlarm();
        void _handleClick();
        void _handleLongPress();
        void _applyClockConfig();
};


#endif