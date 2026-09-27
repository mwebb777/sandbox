#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string>


enum GamepadEventType
{
    eGamepadEvent_None,
    eGamepadEvent_A,
    eGamepadEvent_B,
    eGamepadEvent_X,
    eGamepadEvent_Y,
    eGamepadEvent_Select,
    eGamepadEvent_Start,
    eGamepadEvent_Home,
    eGamepadEvent_TriggerL1,
    eGamepadEvent_TriggerL2,
    eGamepadEvent_TriggerR1,
    eGamepadEvent_TriggerR2,
    eGamepadEvent_LeftJoystickVertical,
    eGamepadEvent_LeftJoystickHorizontal,
    eGamepadEvent_RightJoystickVertical,
    eGamepadEvent_RightJoystickHorizontal,
    eGamepadEvent_PadVertical,
    eGamepadEvent_PadHorizontal
};

struct GamepadEvent
{
    GamepadEventType type = eGamepadEvent_None;
    int value = 0;
    std::string name;
};


class Gamepad
{
public:
    // Get eventNum from:
    //  >cat /proc/bus/input/devices
    //  look for handlers=eventX js0
    //  eventNum = X
    Gamepad(int eventNum);
    ~Gamepad();

    GamepadEvent getEvent();

    std::string eventName(GamepadEvent);

protected:
    int m_eventNum;
    int m_fd;

    input_event m_event;

    GamepadEvent map(input_event event);
};

#endif // GAMEPAD_H
