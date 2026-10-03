/**
 *  adafruitdcmotor.h
 *
 *  MIT License
 *
 *  Copyright (c) 2018, Tom Clarke
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#pragma once

#include "pwm.h"

class AdafruitStepper
{
public:

    static const int kForward = 1;
    static const int kBackward = 2;

    enum Command
    {
        kSingle = 1,
        kDouble = 2,
        kInterleave = 3,
        kMicrostep = 4
    };

    AdafruitStepper (PWM& pwm, int index);

    /** Sets the speed of the motor.
     *  Expects a value between 0 and 255 inclusive.
     */
    void setSpeed (int rpm);

    void step(int steps, int dir, int style = kSingle);

    int onestep(int dir, int style);

private:
    void setPin (int pin, bool enabled);

    void setPwm(int pin, int value);

    PWM& controller;
    int pwmAPin = 0, pwmBPin = 0;
    int inA1Pin = 0, inA2Pin = 0;
    int inB1Pin = 0, inB2Pin = 0;

    int revsteps; // # steps per revolution
    int currentstep;
    int usperstep;

};
