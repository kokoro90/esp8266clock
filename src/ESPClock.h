#ifndef ESPClock_h
#define ESPClock_h

#include "BasicESP8266.h"
#include <OneButton.h>
#include <TM1637Display.h>
#include <NTPClient.h>

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
        const char *_configFileName = "/clockconfig.json";
        struct ClockConfig {
            bool blink = false;
            uint8_t brightness = 3;
            uint16_t alarmTime = 600;
            bool alarmActive = false;
            bool twelveHours = false;
            int tzOffset = 21600;
            bool dst = false;
        };
        ClockConfig _clockConfig;
        enum _state { CLOCK, ALARMTIME, ON, OFF, TIMER };
        enum _state _displayState;
        uint16_t _displayDuration = 3000;
        uint32_t _displayStartTime = 0;
        int _buzzer_pin;
        int _count;
        int _buzzer_state;
        uint32_t _lastUpdated = 0;
        uint32_t _previousTime = 0;
        int _showColon = 128;
        bool _debug;
        bool _alarmOn = false;
        WiFiUDP _ntpUDP;
        NTPClient *_timeClient;
        unsigned long _updateInterval=1800000;

        void _applyClockConfig();
        ClockConfig _createClockConfigFromJson(String json);
        void _saveClockConfig();
        String _getClockConfigJson(bool persist, bool showPersist);
        void _displayTime();
        void _displayAlarmTime();
        void _displayString(String str);
        void _setEndPoints();
        void _handleAlarm();
        void _handleClick();
        void _handleLongPress();
        void _setupClock();
        uint32_t _getEpochTime();
};


#endif