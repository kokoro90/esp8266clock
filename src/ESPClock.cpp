#include <ESPClock.h>
#include <TimeLib.h>

ESPClock::ESPClock(int dio_pin, int clk_pin, int button_pin, int buzzer_pin)
    : _esp(true, -1, true, false, false), _button(button_pin, false, false), _display(clk_pin, dio_pin) {
    _buzzer_pin = buzzer_pin;
    pinMode(_buzzer_pin, OUTPUT);

    _button.attachClick([](void *ctx){
        ((ESPClock*)ctx)->handleClick();
    }, this);

    _esp.begin();
    _count = 0;
    _buzzer_state = LOW;
    _display.setBrightness(7);
    _display.clear();
    _setEndPoints();
}

void ESPClock::button_tick() {
    _button.tick();
}

void ESPClock::displayTime() {
    uint32_t current_time = now();

    if(_lastUpdated < current_time - 60 || _lastUpdated == 0) {
        uint32_t ntp_time = _esp.getEpochTime();
        setTime(ntp_time);
        _lastUpdated = current_time;
    }

    int hours = (current_time % 86400L) / 3600;
    int minutes = (current_time % 3600) / 60;
    int displayTime = hours * 100 + minutes;

    if(displayTime < 100) {
        _display.showNumberDecEx(displayTime, _showColon, true, 4, 0);
    } else {
        _display.showNumberDecEx(displayTime, _showColon, false, 4, 0);
    }
//    _showColon = !_showColon;
}

void ESPClock::handleClick() {
    Serial.println("Button clicked");
    _buzzer_state = !_buzzer_state;
    digitalWrite(_buzzer_pin, _buzzer_state);
//    _display.showNumberDec(++_count);
}

void ESPClock::_setEndPoints() {
    _esp.server->on("/brightness", HTTP_GET, [&](AsyncWebServerRequest *request) {
        if (request->hasParam("level")) {
            int newBrightness = request->getParam("level")->value().toInt();
            _display.setBrightness(newBrightness);
            char brightnessResponse[34] = "Set display brightness to level ";
            char brightnessStr[2];
            strcat(brightnessResponse, itoa(newBrightness, brightnessStr, 10));
            request->send(200, "text/plain", brightnessResponse);
        }
    });

    _esp.server->on("/test", HTTP_GET, [&](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Test response from ESPClock");
    });
}