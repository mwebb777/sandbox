#include <iostream>
#include <chrono>
#include <thread>
#include <lgpio.h> // Include the C lgpio header

// On Raspberry Pi 5, the main 40-pin header maps to gpiochip4
const int GPIO_CHIP = 4;
const int BUTTON_PIN = 23; // Broadcom (BCM) 16

int main() {
    // 1. Open the GPIO chip
    int handle = lgGpiochipOpen(GPIO_CHIP);
    if (handle < 0) {
        std::cerr << "Failed to open gpiochip " << GPIO_CHIP << " (Error: " << handle << ")" << std::endl;
        return 1;
    }

    // 2. Configure line flags for the Pull-Up resistor
    // L_SET_PULL_UP enables the internal pull-up resistor.
    // If you are using an external resistor or pull-down, change this to 0 or L_SET_PULL_DOWN.
    int line_flags = 0;//L_SET_PULL_UP;

    // 3. Claim the line as an input
    int result = lgGpioClaimInput(handle, line_flags, BUTTON_PIN);
    if (result < 0) {
        std::cerr << "Failed to claim GPIO " << BUTTON_PIN << " as input." << std::endl;
        lgGpiochipClose(handle);
        return 1;
    }

    std::cout << "Reading GPIO " << BUTTON_PIN << " (With Pull-Up). Press Ctrl+C to exit..." << std::endl;

    try {
        int last_state = -1;
        while (true) {
            // 4. Read the digital value (returns 0 for LOW, 1 for HIGH, or a negative error code)
            int current_state = lgGpioRead(handle, BUTTON_PIN);

            if (current_state < 0) {
                std::cerr << "Error reading pin state: " << current_state << std::endl;
                break;
            }

            // Only print if the value changes to keep terminal clean
            if (current_state != last_state) {
                std::cout << "Pin State Changed: " << current_state
                          << (current_state == 0 ? " (LOW / Pressed)" : " (HIGH / Released)")
                          << std::endl;
                last_state = current_state;
            }

            // Small delay to prevent maximizing CPU utilization
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    catch (...) {
        // Safe catch block
    }

    // 5. Clean up resources
    lgGpioFree(handle, BUTTON_PIN);
    lgGpiochipClose(handle);
    return 0;
}
