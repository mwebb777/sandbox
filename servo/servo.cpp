#include "servo.h"



Servo::Servo(int num)
{
    m_channel = num-1;

    m_pwm = new Pwm(1, 0x40);

    m_pwm->setPWMFreq(50.0); // 50 Hz for standard servos

    setAngle(0.0f);

}

void Servo::setAngle(float angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    m_angle = angle;
    // 0 deg = ~150 ticks (1.0ms), 180 deg = ~600 ticks (2.0ms) out of 4096
    uint16_t pulse = (uint16_t)(150.0 + (angle / 180.0) * 450.0);
    m_pwm->setPWM(m_channel, 0, pulse);

}
