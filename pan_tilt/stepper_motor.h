#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <QThread>


enum Direction {
    FORWARD = 0,
    BACKWARD = 1
};

class StepperMotor : public QThread
{
    Q_OBJECT
public:
    // 0=first motor, etc
    StepperMotor(int motorNum);

    static bool initialize();
    static void close();

    void enable();
    void disable();

    void step(int steps); // negative is backwards
    void step(Direction dir, int steps);

    int angle();

protected:
    void run() override;

    static int m_handle;

    int m_motorNum;
    int m_delayUsec;
    bool m_enabled;

    int m_steps;
    int m_position;
    Direction m_dir;

    int m_enablePin;
    int m_dirPin;
    int m_stepPin;
};

#endif // STEPPER_MOTOR_H
