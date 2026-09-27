#ifndef SERVO_H
#define SERVO_H


#include "pwm.h"

class Servo
{
public:
    Servo(int num);

    void setAngle(float angle);

protected:

    Pwm* m_pwm;
    int m_channel;
    float m_angle;
};

#endif // SERVO_H
