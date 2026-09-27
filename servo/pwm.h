#pragma once

#include <stdint.h>


class Pwm {
public:
    static const int PWM_ADDRESS = 0x40;

    Pwm(int bus = 1, int address = PWM_ADDRESS);

    void setPWMFreq(float frequency);

    void setPWM(uint8_t channel, uint16_t on, uint16_t off);

    ~Pwm();

private:
    int file;

    void writeByte(uint8_t reg, uint8_t value);

    uint8_t readByte(uint8_t reg);

};
