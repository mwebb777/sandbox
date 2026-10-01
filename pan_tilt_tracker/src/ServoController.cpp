#include "ServoController.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include <unistd.h>
#include <filesystem>

namespace fs = std::filesystem;

ServoController::ServoController(const std::string& pwmchip, int channel,
                                 int min_us, int max_us,
                                 double min_deg, double max_deg)
    : channel_(channel),
      min_us_(min_us), max_us_(max_us),
      min_deg_(min_deg), max_deg_(max_deg),
      current_angle_((min_deg + max_deg) / 2.0)
{
    base_path_ = "/sys/class/pwm/" + pwmchip + "/pwm" + std::to_string(channel);
    exportChannel();
}

ServoController::~ServoController() {
    try {
        if (enabled_) disable();
        unexportChannel();
    } catch (...) {
        // Swallow exceptions in destructor
    }
}

void ServoController::writeSysfs(const std::string& relative_path, const std::string& value) {
    std::string path = base_path_ + "/" + relative_path;
    std::ofstream ofs(path);
    if (!ofs) {
        throw std::runtime_error("Failed to write " + path + " (need root or udev rule?)");
    }
    ofs << value;
    if (!ofs) {
        throw std::runtime_error("Write failed to " + path);
    }
}

std::string ServoController::readSysfs(const std::string& relative_path) {
    std::string path = base_path_ + "/" + relative_path;
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Failed to read " + path);
    }
    std::string value;
    std::getline(ifs, value);
    return value;
}

void ServoController::exportChannel() {
    // Check if already exported
    if (fs::exists(base_path_)) {
        exported_ = true;
        return;
    }

    std::string export_path = base_path_.substr(0, base_path_.find_last_of('/')) + "/export";
    // base_path_ is .../pwmchipX/pwmY → parent is .../pwmchipX
    size_t pos = base_path_.rfind("/pwm");
    std::string chip_path = base_path_.substr(0, pos);
    export_path = chip_path + "/export";

    std::ofstream ofs(export_path);
    if (!ofs) {
        throw std::runtime_error("Cannot export PWM channel. Is dtoverlay=pwm-2chan enabled? "
                                 "Run as root? Path: " + export_path);
    }
    ofs << channel_;
    ofs.close();

    // Give udev a moment
    usleep(100000);
    if (!fs::exists(base_path_)) {
        throw std::runtime_error("PWM channel exported but path does not exist: " + base_path_);
    }
    exported_ = true;
}

void ServoController::unexportChannel() {
    if (!exported_) return;
    size_t pos = base_path_.rfind("/pwm");
    std::string chip_path = base_path_.substr(0, pos);
    std::string unexport_path = chip_path + "/unexport";
    try {
        std::ofstream ofs(unexport_path);
        if (ofs) ofs << channel_;
    } catch (...) {}
    exported_ = false;
}

void ServoController::enable() {
    if (enabled_) return;
    writeSysfs("period", std::to_string(PERIOD_NS));
    // Start at center
    setAngle(current_angle_);
    writeSysfs("enable", "1");
    enabled_ = true;
}

void ServoController::disable() {
    if (!enabled_) return;
    writeSysfs("enable", "0");
    enabled_ = false;
}

int ServoController::angleToDutyNs(double degrees) const {
    degrees = std::clamp(degrees, min_deg_, max_deg_);
    double ratio = (degrees - min_deg_) / (max_deg_ - min_deg_);
    int pulse_us = static_cast<int>(min_us_ + ratio * (max_us_ - min_us_));
    return pulse_us * 1000;  // convert µs → ns
}

void ServoController::setAngle(double degrees) {
    degrees = std::clamp(degrees, min_deg_, max_deg_);
    int duty_ns = angleToDutyNs(degrees);
    writeSysfs("duty_cycle", std::to_string(duty_ns));
    current_angle_ = degrees;
}

void ServoController::moveBy(double delta_degrees) {
    setAngle(current_angle_ + delta_degrees);
}

void ServoController::center() {
    setAngle((min_deg_ + max_deg_) / 2.0);
}
