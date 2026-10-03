/**
 *  adafruitdcmotor.cpp
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

#include "adafruit_stepper.h"
#include "util.h"

#include <inttypes.h>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>

#define MICROSTEPS 16 // 8 or 16

#if (MICROSTEPS == 8)
///! A sinusoial microstepping curve for the PWM output (8-bit range) with 9
/// points - last one is start of next step.
static uint8_t microstepcurve[] = {0, 50, 98, 142, 180, 212, 236, 250, 255};
#elif (MICROSTEPS == 16)
///! A sinusoial microstepping curve for the PWM output (8-bit range) with 17
/// points - last one is start of next step.
static uint8_t microstepcurve[] = {0,   25,  50,  74,  98,  120, 141, 162, 180,
                                   197, 212, 225, 236, 244, 250, 253, 255};
#endif


AdafruitStepper::AdafruitStepper (PWM& pwm, int index)
    : controller (pwm)
{
    revsteps = currentstep = 0;

    switch (index)
    {
    case 0:
        pwmAPin = 8;
        inA2Pin = 9;
        inA1Pin = 10;
        pwmBPin = 13;
        inB2Pin = 12;
        inB1Pin = 11;
        break;
    case 1:
        pwmAPin = 2;
        inA2Pin = 3;
        inA1Pin = 4;
        pwmBPin = 7;
        inB2Pin = 6;
        inB1Pin = 5;
        break;
    default:
        log::error ("Motor index out-of-range. Must be between 0 and 1 inclusive.");
        break;
    }
}


void AdafruitStepper::setPwm(int pin, int value)
{
    if (value > 4095) {
        controller.setChannel(pin, 4096, 0);
    } else
        controller.setChannel(pin, 0, value);
}

void AdafruitStepper::setSpeed (int rpm)
{
    usperstep = 60000000 / ((uint32_t)revsteps * (uint32_t)rpm);
}

void AdafruitStepper::step(int steps, int dir, int style)
{
    uint32_t uspers = usperstep;

    if (style == kInterleave) {
        uspers /= 2;
    } else if (style == kMicrostep) {
        uspers /= MICROSTEPS;
        steps *= MICROSTEPS;
    }

    while (steps--) {
        onestep(dir, style);
        std::this_thread::sleep_for(std::chrono::microseconds(uspers));
    }
}

int AdafruitStepper::onestep(int dir, int style)
{
    uint8_t ocrb, ocra;

    ocra = ocrb = 255;

    // next determine what sort of stepping procedure we're up to
    if (style == kSingle) {
        if ((currentstep / (MICROSTEPS / 2)) % 2) { // we're at an odd step, weird
            if (dir == kForward) {
                currentstep += MICROSTEPS / 2;
            } else {
                currentstep -= MICROSTEPS / 2;
            }
        } else { // go to the next even step
            if (dir == kForward) {
                currentstep += MICROSTEPS;
            } else {
                currentstep -= MICROSTEPS;
            }
        }
    } else if (style == kDouble) {
        if (!(currentstep / (MICROSTEPS / 2) % 2)) { // we're at an even step, weird
            if (dir == kForward) {
                currentstep += MICROSTEPS / 2;
            } else {
                currentstep -= MICROSTEPS / 2;
            }
        } else { // go to the next odd step
            if (dir == kForward) {
                currentstep += MICROSTEPS;
            } else {
                currentstep -= MICROSTEPS;
            }
        }
    } else if (style == kInterleave) {
        if (dir == kForward) {
            currentstep += MICROSTEPS / 2;
        } else {
            currentstep -= MICROSTEPS / 2;
        }
    }

    if (style == kMicrostep) {
        if (dir == kForward) {
            currentstep++;
        } else {
            // BACKWARDS
            currentstep--;
        }

        currentstep += MICROSTEPS * 4;
        currentstep %= MICROSTEPS * 4;

        ocra = ocrb = 0;
        if (currentstep < MICROSTEPS) {
            ocra = microstepcurve[MICROSTEPS - currentstep];
            ocrb = microstepcurve[currentstep];
        } else if ((currentstep >= MICROSTEPS) && (currentstep < MICROSTEPS * 2)) {
            ocra = microstepcurve[currentstep - MICROSTEPS];
            ocrb = microstepcurve[MICROSTEPS * 2 - currentstep];
        } else if ((currentstep >= MICROSTEPS * 2) &&
                   (currentstep < MICROSTEPS * 3)) {
            ocra = microstepcurve[MICROSTEPS * 3 - currentstep];
            ocrb = microstepcurve[currentstep - MICROSTEPS * 2];
        } else if ((currentstep >= MICROSTEPS * 3) &&
                   (currentstep < MICROSTEPS * 4)) {
            ocra = microstepcurve[currentstep - MICROSTEPS * 3];
            ocrb = microstepcurve[MICROSTEPS * 4 - currentstep];
        }
    }

    currentstep += MICROSTEPS * 4;
    currentstep %= MICROSTEPS * 4;

    setPwm(pwmAPin, ocra * 16);
    setPwm(pwmBPin, ocrb * 16);

    // release all
    uint8_t latch_state = 0; // all motor pins to 0

    // Serial.println(step, DEC);
    if (style == kMicrostep) {
        if (currentstep < MICROSTEPS)
            latch_state |= 0x03;
        if ((currentstep >= MICROSTEPS) && (currentstep < MICROSTEPS * 2))
            latch_state |= 0x06;
        if ((currentstep >= MICROSTEPS * 2) && (currentstep < MICROSTEPS * 3))
            latch_state |= 0x0C;
        if ((currentstep >= MICROSTEPS * 3) && (currentstep < MICROSTEPS * 4))
            latch_state |= 0x09;
    } else {
        switch (currentstep / (MICROSTEPS / 2)) {
        case 0:
            latch_state |= 0x1; // energize coil 1 only
            break;
        case 1:
            latch_state |= 0x3; // energize coil 1+2
            break;
        case 2:
            latch_state |= 0x2; // energize coil 2 only
            break;
        case 3:
            latch_state |= 0x6; // energize coil 2+3
            break;
        case 4:
            latch_state |= 0x4; // energize coil 3 only
            break;
        case 5:
            latch_state |= 0xC; // energize coil 3+4
            break;
        case 6:
            latch_state |= 0x8; // energize coil 4 only
            break;
        case 7:
            latch_state |= 0x9; // energize coil 1+4
            break;
        }
    }

    if (latch_state & 0x1) {
        setPin(inA2Pin, true);
    } else {
        setPin(inA2Pin, false);
    }
    if (latch_state & 0x2) {
        setPin(inB1Pin, true);
    } else {
        setPin(inB1Pin, false);
    }
    if (latch_state & 0x4) {
        setPin(inA1Pin, true);
    } else {
        setPin(inA1Pin, false);
    }
    if (latch_state & 0x8) {
        setPin(inB2Pin, true);
    } else {
        setPin(inB2Pin, false);
    }

    return currentstep;

}


void AdafruitStepper::setPin (int pin, bool enabled)
{
    if (pin < 0 || pin > 15)
    {
        log::error ("Failed to set PWM pin " + std::to_string (pin) + ". Must be between 0 and 15 inclusive.");
        return;
    }

    controller.setChannel (pin, enabled ? 4096 : 0, enabled ? 0 : 4096);
}
