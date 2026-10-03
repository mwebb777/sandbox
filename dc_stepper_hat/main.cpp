//#define DC
#ifdef DC

#include <thread>
#include <chrono>
#include "adafruit_motorhat.h"

int main()
{
    using namespace std::chrono_literals;

    // connect using the default device address 0x60
    AdafruitMotorHAT hat;

    auto motor1 = hat.getMotor(1);
    auto motor2 = hat.getMotor(2);
    auto motor3 = hat.getMotor(3);
    auto motor4 = hat.getMotor(4);

    // get the motor connected to port 1
    {
        // speed must be set before running commands
        motor1->setSpeed (255);
        motor2->setSpeed (255);
        motor3->setSpeed (255);
        motor4->setSpeed (255);

        motor1->run (AdafruitDCMotor::kForward);
        motor2->run (AdafruitDCMotor::kForward);
        motor3->run (AdafruitDCMotor::kForward);
        motor4->run (AdafruitDCMotor::kForward);
        std::this_thread::sleep_for (1s);

        motor1->run (AdafruitDCMotor::kBackward);
        motor2->run (AdafruitDCMotor::kBackward);
        motor3->run (AdafruitDCMotor::kBackward);
        motor4->run (AdafruitDCMotor::kBackward);
        std::this_thread::sleep_for (1s);

        // release the motor after use
        motor1->run (AdafruitDCMotor::kRelease);
        motor2->run (AdafruitDCMotor::kRelease);
        motor3->run (AdafruitDCMotor::kRelease);
        motor4->run (AdafruitDCMotor::kRelease);
    }

    return 0;
}


#else


#include <thread>
#include <chrono>
#include "adafruit_motorhat.h"

int main()
{
    using namespace std::chrono_literals;

    // connect using the default device address 0x60
    AdafruitMotorHAT hat;

    auto stepper = hat.getStepper(180, 2);

    stepper->setSpeed(60); // 10 rpm

    //stepper->step(1000, AdafruitStepper::kForward, AdafruitStepper::kSingle);
    //stepper->step(1000, AdafruitStepper::kBackward, AdafruitStepper::kSingle);

    stepper->step(1000, AdafruitStepper::kForward, AdafruitStepper::kDouble);
    stepper->step(1000, AdafruitStepper::kBackward, AdafruitStepper::kDouble);

    //stepper->step(100, AdafruitStepper::kForward, AdafruitStepper::kInterleave);
    //stepper->step(100, AdafruitStepper::kBackward, AdafruitStepper::kInterleave);

    //stepper->step(50, AdafruitStepper::kForward, AdafruitStepper::kMicrostep);
    //stepper->step(50, AdafruitStepper::kBackward, AdafruitStepper::kMicrostep);


    return 0;
}




#endif
