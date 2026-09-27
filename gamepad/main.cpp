#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string>

#include "gamepad.h"


int main() {

    Gamepad gamepad(7);

    GamepadEvent ev;

    while (true) {

        ev = gamepad.getEvent();

        if (ev.type != eGamepadEvent_None)
            std::cout << "Event: " << ev.name << " = " << ev.value << std::endl;
    }


    return 0;
}
