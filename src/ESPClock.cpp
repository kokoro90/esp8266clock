#include <ESPClock.h>

//ESPClock::ESPClock(): _esp(false, -1, true, false, false) {
//    _esp.begin();
//}

ESPClock::ESPClock(int dio_pin, int clk_pin, int button_pin, int buzzer_pin)
    : _esp(false, -1, true, false, false), _button(button_pin, false, false), _display(clk_pin, dio_pin) {
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
    _display.showNumberDec(_count);
}

void ESPClock::button_tick() {
    _button.tick();
}

void ESPClock::handleClick() {
    _buzzer_state = !_buzzer_state;
    digitalWrite(_buzzer_pin, _buzzer_state);
    _display.showNumberDec(++_count);
}
