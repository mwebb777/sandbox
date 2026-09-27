#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string>

// Modify this path to match your controller's event ID found in Step 2
const std::string DEVICE_PATH = "/dev/input/event7";

int main() {
    // Open the event file in read-only mode
    int fd = open(DEVICE_PATH.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Error: Could not open device at " << DEVICE_PATH
                  << ". Did you run as root/sudo?" << std::endl;
        return 1;
    }

    std::cout << "Successfully connected to PS3 Controller! Listening for inputs..." << std::endl;

    struct input_event ev;

    while (true) {
        ssize_t bytes = read(fd, &ev, sizeof(struct input_event));
        if (bytes < (ssize_t)sizeof(struct input_event)) {
            std::cerr << "Error reading event or controller disconnected." << std::endl;
            break;
        }

        // Filter events
        if (ev.type == EV_KEY) { // Button press/release
            std::cout << "Button Code: " << ev.code
                      << " | State: " << (ev.value ? "Pressed" : "Released")
                      << std::endl;

            // Example mapping: Cross (X) button is typically 304 (BTN_SOUTH)
            if (ev.code == BTN_SOUTH && ev.value == 1) {
                std::cout << " -> You pressed the Cross (X) button!" << std::endl;
            }
        }
        else if (ev.type == EV_ABS) { // Analog Joysticks or D-Pad
            // ev.code represents the axis (0: Left Stick X, 1: Left Stick Y)
            // ev.value ranges from 0 to 255 (128 is center for joysticks)

            std::cout << "Axis Code: " << (int)ev.code
                      << " | Position: " << ev.value << std::endl;

        }
    }

    close(fd);
    return 0;
}
