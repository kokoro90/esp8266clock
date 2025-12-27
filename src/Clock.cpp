#include <Clock.h>

Clock::Clock(int dio_pin, int clk_pin, int button_pin, int buzzer_pin)
    : _esp(false, -1, true, false, false), _display(clk_pin, dio_pin), _button(button_pin, false, false) {
    _buzzer_pin = buzzer_pin;
    pinMode(_buzzer_pin, OUTPUT);

    _button.attachClick([](void *ctx){
        ((Clock*)ctx)->handleClick();
    }, this);

    _esp.begin();
    _count = 0;
    _buzzer_state = LOW;
    _display.setBrightness(7);
    _display.clear();
    _display.showNumberDec(_count);
}

void Clock::button_tick() {
    _button.tick();
}

void Clock::handleClick() {
    _buzzer_state = !_buzzer_state;
    digitalWrite(_buzzer_pin, _buzzer_state);
    _display.showNumberDec(++_count);
}