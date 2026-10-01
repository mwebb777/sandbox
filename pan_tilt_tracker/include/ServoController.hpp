#pragma once

#include <string>
#include <stdexcept>

/**
 * Controls a hobby servo via Linux kernel PWM sysfs interface.
 * Compatible with Raspberry Pi 5 (RP1) when dtoverlay=pwm-2chan is enabled.
 *
 * Pulse width mapping (typical):
 *   500 µs  → ~0°
 *  1500 µs  → 90° (center)
 *  2500 µs  → 180°
 *
 * Period is fixed at 20 ms (50 Hz).
 */
class ServoController {
public:
    /**
     * @param pwmchip   e.g. "pwmchip0" or "pwmchip2"
     * @param channel   0 or 1
     * @param min_us    Minimum pulse width in microseconds
     * @param max_us    Maximum pulse width in microseconds
     * @param min_deg   Angle corresponding to min_us
     * @param max_deg   Angle corresponding to max_us
     */
    ServoController(const std::string& pwmchip, int channel,
                    int min_us = 500, int max_us = 2500,
                    double min_deg = 0.0, double max_deg = 180.0);

    ~ServoController();

    // Non-copyable
    ServoController(const ServoController&) = delete;
    ServoController& operator=(const ServoController&) = delete;

    void enable();
    void disable();
    bool isEnabled() const { return enabled_; }

    /**
     * Set absolute angle in degrees. Clamped to [min_deg, max_deg].
     */
    void setAngle(double degrees);

    /**
     * Relative move (useful for velocity-style control).
     */
    void moveBy(double delta_degrees);

    double getAngle() const { return current_angle_; }
    double getMinDeg() const { return min_deg_; }
    double getMaxDeg() const { return max_deg_; }

    /** Center the servo (mid-point of range). */
    void center();

private:
    void writeSysfs(const std::string& relative_path, const std::string& value);
    std::string readSysfs(const std::string& relative_path);
    void exportChannel();
    void unexportChannel();
    int angleToDutyNs(double degrees) const;

    std::string base_path_;   // /sys/class/pwm/pwmchipX/pwmY
    int channel_;
    int min_us_, max_us_;
    double min_deg_, max_deg_;
    double current_angle_;
    bool enabled_ = false;
    bool exported_ = false;

    static constexpr int PERIOD_NS = 20'000'000;  // 20 ms = 50 Hz
};
