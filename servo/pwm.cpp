#include "pwm.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <inttypes.h>
#include <math.h>
#include <iostream>

#define MODE1           0x00
#define PRESCALE        0xFE
#define LED0_ON_L       0x06


void Pwm::writeByte(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(file, buf, 2) != 2) {
        std::cerr << "Error writing to I2C slave" << std::endl;
    }
}

uint8_t Pwm::readByte(uint8_t reg) {
    if (write(file, &reg, 1) != 1) {
        std::cerr << "Error establishing register for read" << std::endl;
    }
    uint8_t value;
    if (read(file, &value, 1) != 1) {
        std::cerr << "Error reading from I2C slave" << std::endl;
    }
    return value;
}

Pwm::Pwm(int bus, int address) {
    char filename[20];
    snprintf(filename, 19, "/dev/i2c-%d", bus);
    file = open(filename, O_RDWR);
    if (file < 0) {
        std::cerr << "Failed to open I2C bus" << std::endl;
        return;
    }
    if (ioctl(file, I2C_SLAVE, address) < 0) {
        std::cerr << "Failed to acquire bus access and/or talk to slave" << std::endl;
    }
    // Initialize mode 1 (awake)
    writeByte(MODE1, 0x01);
    usleep(10000);
}

void Pwm::setPWMFreq(float frequency) {
    float prescaleval = 25000000.0; // 25MHz oscillator
    prescaleval /= 4096.0;         // 12-bit
    prescaleval /= frequency;
    prescaleval -= 1.0;
    uint8_t prescale = (uint8_t)(prescaleval + 0.5);

    uint8_t oldmode = readByte(MODE1);
    uint8_t newmode = (oldmode & 0x7F) | 0x10; // Sleep
    writeByte(MODE1, newmode);
    writeByte(PRESCALE, prescale);
    writeByte(MODE1, oldmode);
    usleep(10000);
    writeByte(MODE1, oldmode | 0xa1); // Auto-increment enabled
}

void Pwm::setPWM(uint8_t channel, uint16_t on, uint16_t off) {
    uint8_t reg = LED0_ON_L + 4 * channel;
    writeByte(reg, on & 0xFF);
    writeByte(reg + 1, on >> 8);
    writeByte(reg + 2, off & 0xFF);
    writeByte(reg + 3, off >> 8);
}

Pwm::~Pwm() {
    close(file);
}
