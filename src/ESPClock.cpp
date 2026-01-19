#include <ESPClock.h>
#include <TimeLib.h>

ESPClock::ESPClock(bool debug, int dio_pin, int clk_pin, int button_pin, int buzzer_pin)
    : _esp(false, 100, true, false, false), _button(button_pin, false, false), _display(clk_pin, dio_pin) {
    _debug = debug;
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

    if(_lastUpdated < current_time - (_esp.getUpdateInterval() / 1000) || _lastUpdated == 0) {
        uint32_t ntp_time = _esp.getEpochTime();
        setTime(ntp_time);
        _lastUpdated = current_time;

        if(_debug) {
            Serial.print("Retrieved time from NTP server: ");
            Serial.print((current_time % 86400L) / 3600);
            Serial.print(":");
            Serial.print((current_time % 3600) / 60 / 10);
            Serial.print(((current_time % 3600) / 60) % 10);
        }
    }

    uint8_t clock_data[4];
    int hours = (current_time % 86400L) / 3600;
    clock_data[0] = hours >= 10 ? _display.encodeDigit(hours / 10) : 0;
    clock_data[1] = _display.encodeDigit(hours % 10) | _showColon;

    int minutes = (current_time % 3600) / 60;
    clock_data[2] = _display.encodeDigit(minutes / 10);
    clock_data[3] = _display.encodeDigit(minutes % 10);

    _display.setSegments(clock_data);

    if(_blink) {
        _showColon = _showColon == 128 ? 0 : 128;
    } else {
        _showColon = 128;
    }
}

void ESPClock::handleClick() {
    if(_debug) Serial.println("Button clicked");
    _buzzer_state = !_buzzer_state;
    digitalWrite(_buzzer_pin, _buzzer_state);
//     _display.showNumberDec(++_count);
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

    _esp.server->on("/blink/on", HTTP_GET, [&](AsyncWebServerRequest *request) {
        _blink = true;
        request->send(200, "text/pain", "Turning blinking colons on");
    });

    _esp.server->on("/blink/off", HTTP_GET, [&](AsyncWebServerRequest *request) {
        _blink = false;
        request->send(200, "text/pain", "Turning blinking colons off");
    });

    _esp.server->on("/test", HTTP_GET, [&](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Test response from ESPClock");
    });
}