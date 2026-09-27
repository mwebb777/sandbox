#include <iostream>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define PCA9685_ADDRESS 0x40
#define MODE1           0x00
#define PRESCALE        0xFE
#define LED0_ON_L       0x06

class PCA9685 {
private:
    int file;

    void writeByte(uint8_t reg, uint8_t value) {
        uint8_t buf[2] = {reg, value};
        if (write(file, buf, 2) != 2) {
            std::cerr << "Error writing to I2C slave" << std::endl;
        }
    }

    uint8_t readByte(uint8_t reg) {
        if (write(file, &reg, 1) != 1) {
            std::cerr << "Error establishing register for read" << std::endl;
        }
        uint8_t value;
        if (read(file, &value, 1) != 1) {
            std::cerr << "Error reading from I2C slave" << std::endl;
        }
        return value;
    }

public:
    PCA9685(int bus = 1, int address = PCA9685_ADDRESS) {
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

    void setPWMFreq(float frequency) {
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

    void setPWM(uint8_t channel, uint16_t on, uint16_t off) {
        uint8_t reg = LED0_ON_L + 4 * channel;
        writeByte(reg, on & 0xFF);
        writeByte(reg + 1, on >> 8);
        writeByte(reg + 2, off & 0xFF);
        writeByte(reg + 3, off >> 8);
    }

    // Map angle (0 to 180 degrees) to pulse length
    void setServoAngle(uint8_t channel, float angle) {
        if (angle < 0) angle = 0;
        if (angle > 180) angle = 180;
        // 0 deg = ~150 ticks (1.0ms), 180 deg = ~600 ticks (2.0ms) out of 4096
        uint16_t pulse = (uint16_t)(150.0 + (angle / 180.0) * 450.0);
        setPWM(channel, 0, pulse);
    }

    ~PCA9685() {
        close(file);
    }
};

int main() {
    std::cout << "Initializing Waveshare Servo Driver HAT..." << std::endl;
    PCA9685 pwm(1, 0x40);

    pwm.setPWMFreq(50.0); // 50 Hz for standard servos

    std::cout << "Moving servo on channel 0 to 0 degrees" << std::endl;
    pwm.setServoAngle(0, 0);
    sleep(1);

    std::cout << "Moving servo on channel 0 to 90 degrees" << std::endl;
    pwm.setServoAngle(0, 90);
    sleep(1);

    std::cout << "Moving servo on channel 0 to 180 degrees" << std::endl;
    pwm.setServoAngle(0, 180);
    sleep(1);

    return 0;
}
