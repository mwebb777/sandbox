#include "stepper_motor.h"

#ifdef Q_OS_LINUX
#include <iostream>
#include <unistd.h>
#include <lgpio.h>
#endif

int StepperMotor::m_handle = 0;


StepperMotor::StepperMotor(int motorNum)
{
    m_steps = 0;
    m_dir = FORWARD;
    m_position = 0;

    if (motorNum == 0)
    {
        // Waveshare Stepper Motor 1 BCM Pin configurations
        m_dirPin     = 13;
        m_stepPin   = 19;
        m_enablePin = 30;//12 - enable pins don't work, map to ground
    }
    else
    {
        // Waveshare Stepper Motor 2 BCM Pin configurations
        m_dirPin    = 24;
        m_stepPin   = 18;
        m_enablePin = 39;//4 - enable pins don't work, map to ground
    }

#ifdef Q_OS_LINUX
    // Claim lines as output pins
    // 0 defines the flags (default options)
    if (lgGpioClaimOutput(m_handle, 0, m_dirPin, 0) < 0 ||
        lgGpioClaimOutput(m_handle, 0, m_stepPin, 0) < 0 ||
        lgGpioClaimOutput(m_handle, 0, m_enablePin, 0) < 0) {
        std::cerr << "Failed to claim GPIO output pins." << std::endl;
        lgGpiochipClose(m_handle);
    }
#endif
}

bool StepperMotor::initialize()
{
#ifdef Q_OS_LINUX
    // Open the primary GPIO chip (usually chip 0 on Raspberry Pi)
    // lgGpiochipOpen returns a unique integer handle if successful
    m_handle = lgGpiochipOpen(0);
    if (m_handle < 0) {
        std::cerr << "Failed to open GPIO chip. Error code: " << m_handle << std::endl;
        return false;
    }
#endif

    return true;
}

void StepperMotor::close()
{
#ifdef Q_OS_LINUX
    // Clean up and close the GPIO chip link
    lgGpiochipClose(m_handle);
#endif
}

void StepperMotor::enable()
{
    m_enabled = true;

#ifdef Q_OS_LINUX
    // Enable the DRV8825 driver (Pulling ENABLE pin LOW activates the motor)
    lgGpioWrite(m_handle, m_enablePin, 0);

#endif
}

void StepperMotor::disable()
{
    m_enabled = false;

#ifdef Q_OS_LINUX
    // Enable the DRV8825 driver (Pulling ENABLE pin HIGH disables the motor)
    lgGpioWrite(m_handle, m_enablePin, 1);
#endif
}

void StepperMotor::step(Direction dir, int steps)
{
    if (m_enabled)
    {
        if (dir == FORWARD)
            m_position += steps;
        else
            m_position -= steps;

        if (m_position < 0)
            m_position += 360;
        if (m_position >= 360)
            m_position -= 360;

        m_dir = dir;
        m_steps = steps;
    }
}

int StepperMotor::angle()
{
    return m_position;
}

void StepperMotor::step(int steps)
{
    Direction dir = steps < 0 ? BACKWARD : FORWARD;
    steps = std::abs(steps);
    step(dir, steps);
}

void StepperMotor::run()
{

#ifdef Q_OS_LINUX
    while(!isInterruptionRequested())
    {
        // Wait for a new command
        if (m_steps < 0 || !m_enabled)
        {
            usleep(step_delay_us);
            continue;
        }

        // Set direction pin value: 0 for FORWARD, 1 for BACKWARD
        lgGpioWrite(m_handle, m_dirPin, static_cast<int>(m_dir));

        std::cout << "Moving " << (m_dir == FORWARD ? "Forward" : "Backward")
                  << " for " << steps << " steps..." << std::endl;

        // Generate step pulses
        for (int i = 0; i < m_steps; ++i)
        {
            lgGpioWrite(handle, m_stepPin, 1); // Pulse HIGH
            usleep(step_delay_us);
            lgGpioWrite(handle, m_stepPin, 0); // Pulse LOW
            usleep(step_delay_us);
        }

        m_steps = -1;
    }
#endif
}
