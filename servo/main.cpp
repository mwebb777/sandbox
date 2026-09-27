#include <iostream>
#include <stdint.h>
#include <unistd.h>

#include "servo.h"

int main() {
    std::cout << "Initializing Waveshare Servo Driver HAT..." << std::endl;
    Servo servo(1);
    Servo servo2(2);

    std::cout << "Moving servo on channel 0 to 0 degrees" << std::endl;
    servo.setAngle(0.0);
    servo2.setAngle(0.0);
    sleep(1);

    std::cout << "Moving servo on channel 0 to 90 degrees" << std::endl;
    servo.setAngle(90.0);
    servo2.setAngle(90.0);
    sleep(1);

    std::cout << "Moving servo on channel 0 to 180 degrees" << std::endl;
    servo.setAngle(180);
    servo2.setAngle(180);
    sleep(1);

    return 0;
}
