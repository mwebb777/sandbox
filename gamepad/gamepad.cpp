#include "gamepad.h"


struct GamepadMapData
{
    int type;
    int code;
    GamepadEventType gamepadType;
    std::string name;
};

GamepadMapData gamepadMap[] = {
    {0, 0, eGamepadEvent_None, "None"},
    {EV_KEY, 304, eGamepadEvent_A, "A"},
    {EV_KEY, 305, eGamepadEvent_B, "B"},
    {EV_KEY, 307, eGamepadEvent_X, "X"},
    {EV_KEY, 308, eGamepadEvent_Y, "Y"},
    {EV_KEY, 314, eGamepadEvent_Select, "Select"},
    {EV_KEY, 315, eGamepadEvent_Start, "Start"},
    {EV_KEY, 316, eGamepadEvent_Home, "Home"},
    {EV_KEY, 310, eGamepadEvent_TriggerL1, "Trigger L1"},
    {EV_KEY, 312, eGamepadEvent_TriggerL2, "Trigger L2"},
    {EV_KEY, 311, eGamepadEvent_TriggerR1, "Trigger R1"},
    {EV_KEY, 313, eGamepadEvent_TriggerR2, "Trigger R2"},
    {EV_ABS, 1, eGamepadEvent_LeftJoystickVertical, "Left Joystick Vert"},
    {EV_ABS, 0, eGamepadEvent_LeftJoystickHorizontal, "Left Joystick Horz"},
    {EV_ABS, 5, eGamepadEvent_RightJoystickVertical, "Right Joystick Vert"},
    {EV_ABS, 2, eGamepadEvent_RightJoystickHorizontal, "Right Joystick Horz"},
    {EV_ABS, 17, eGamepadEvent_PadVertical, "Pad Vert"},
    {EV_ABS, 16, eGamepadEvent_PadHorizontal, "Pad Horz"}
};


Gamepad::Gamepad(int eventNum)
{
    m_eventNum = eventNum;

    std::string devicePath = "/dev/input/event";
    devicePath += std::to_string(eventNum);

    m_fd = open(devicePath.c_str(), O_RDONLY);
    if (m_fd < 0) {
        std::cerr << "Error: Could not open device at " << devicePath
                  << ". Did you run as root/sudo?" << std::endl;
    }

    std::cout << "Gamepad initialized: event=" << eventNum << std::endl;
}

Gamepad::~Gamepad()
{
    close(m_fd);
}

GamepadEvent Gamepad::getEvent()
{
    GamepadEvent ret;

    if (m_fd < 0)
        return ret;

    input_event ev;
    ssize_t bytes = read(m_fd, &ev, sizeof(struct input_event));
    if (bytes < (ssize_t)sizeof(struct input_event)) {
        std::cerr << "Error reading event or controller disconnected." << std::endl;
        return ret;
    }

    ret = map(ev);
    return ret;
}

GamepadEvent Gamepad::map(input_event ev)
{
    GamepadEvent ret;

    for (auto gp : gamepadMap)
    {
        if (gp.type == ev.type && gp.code == ev.code)
        {
            ret.type = gp.gamepadType;
            ret.value = ev.value;
            ret.name = gp.name;
            break;
        }
    }
    return ret;
}
