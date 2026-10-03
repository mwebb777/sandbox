#include <iostream>
#include <unistd.h>
#include <lgpio.h>

// Waveshare Stepper Motor 1 BCM Pin configurations
const int M1_DIR_PIN    = 13;
const int M1_STEP_PIN   = 19;
const int M1_ENABLE_PIN = 30;//12 - enable pins don't work, map to ground

// Waveshare Stepper Motor 2 BCM Pin configurations
const int M2_DIR_PIN    = 24;
const int M2_STEP_PIN   = 18;
const int M2_ENABLE_PIN = 39;//4 - enable pins don't work, map to ground

enum Direction {
    FORWARD = 0,
    BACKWARD = 1
};

// Function to control motor pulses using lgpio
void turn_step(int handle, Direction dir, int steps, int step_delay_us) {
    // Set direction pin value: 0 for FORWARD, 1 for BACKWARD
    lgGpioWrite(handle, M1_DIR_PIN, static_cast<int>(dir));
    lgGpioWrite(handle, M2_DIR_PIN, static_cast<int>(dir));

    std::cout << "Moving " << (dir == FORWARD ? "Forward" : "Backward")
              << " for " << steps << " steps..." << std::endl;

    // Generate step pulses
    for (int i = 0; i < steps; ++i) {
        lgGpioWrite(handle, M1_STEP_PIN, 1); // Pulse HIGH
        lgGpioWrite(handle, M2_STEP_PIN, 1); // Pulse HIGH
        usleep(step_delay_us);
        lgGpioWrite(handle, M1_STEP_PIN, 0); // Pulse LOW
        lgGpioWrite(handle, M2_STEP_PIN, 0); // Pulse LOW
        usleep(step_delay_us);
    }
}

int main() {
    // Open the primary GPIO chip (usually chip 0 on Raspberry Pi)
    // lgGpiochipOpen returns a unique integer handle if successful
    int gpio_handle = lgGpiochipOpen(0);
    if (gpio_handle < 0) {
        std::cerr << "Failed to open GPIO chip. Error code: " << gpio_handle << std::endl;
        return 1;
    }

    // Claim lines as output pins
    // 0 defines the flags (default options)
    if (lgGpioClaimOutput(gpio_handle, 0, M1_DIR_PIN, 0) < 0 ||
        lgGpioClaimOutput(gpio_handle, 0, M1_STEP_PIN, 0) < 0 ||
        lgGpioClaimOutput(gpio_handle, 0, M1_ENABLE_PIN, 0) < 0) {
        std::cerr << "Failed to claim GPIO output pins." << std::endl;
        lgGpiochipClose(gpio_handle);
        return 1;
    }

    // Claim lines as output pins
    // 0 defines the flags (default options)
    if (lgGpioClaimOutput(gpio_handle, 0, M2_DIR_PIN, 0) < 0 ||
        lgGpioClaimOutput(gpio_handle, 0, M2_STEP_PIN, 0) < 0 ||
        lgGpioClaimOutput(gpio_handle, 0, M2_ENABLE_PIN, 0) < 0) {
        std::cerr << "Failed to claim GPIO output pins." << std::endl;
        lgGpiochipClose(gpio_handle);
        return 1;
    }

    // Enable the DRV8825 driver (Pulling ENABLE pin LOW activates the motor)
    lgGpioWrite(gpio_handle, M1_ENABLE_PIN, 0);
    lgGpioWrite(gpio_handle, M2_ENABLE_PIN, 0);

    // Execute movements
    turn_step(gpio_handle, FORWARD, 2000, 300);
    sleep(1);

    turn_step(gpio_handle, BACKWARD, 2000, 300);
    sleep(1);

    // Disable the driver chip (Pull ENABLE HIGH) to prevent overheating when idle
    lgGpioWrite(gpio_handle, M1_ENABLE_PIN, 1);
    lgGpioWrite(gpio_handle, M2_ENABLE_PIN, 1);
    std::cout << "Motor disabled safely." << std::endl;

    // Clean up and close the GPIO chip link
    lgGpiochipClose(gpio_handle);
    return 0;
}
