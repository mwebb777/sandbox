#include "PIDController.hpp"
#include <algorithm>
#include <cmath>

PIDController::PIDController(double kp, double ki, double kd,
                             double output_min, double output_max)
    : kp_(kp), ki_(ki), kd_(kd),
      output_min_(output_min), output_max_(output_max) {}

void PIDController::setGains(double kp, double ki, double kd) {
    kp_ = kp;
    ki_ = ki;
    kd_ = kd;
}

void PIDController::setOutputLimits(double min, double max) {
    output_min_ = min;
    output_max_ = max;
}

void PIDController::setSetpoint(double setpoint) {
    setpoint_ = setpoint;
}

void PIDController::reset() {
    integral_ = 0.0;
    last_error_ = 0.0;
    first_run_ = true;
}

double PIDController::update(double measurement, double dt) {
    if (dt <= 0.0) dt = 0.033;  // fallback ~30 Hz

    double error = setpoint_ - measurement;

    // Proportional
    double p = kp_ * error;

    // Integral with simple anti-windup (clamp integral contribution)
    integral_ += error * dt;
    double i = ki_ * integral_;

    // Derivative (on measurement to avoid derivative kick)
    double d = 0.0;
    if (!first_run_) {
        d = kd_ * (error - last_error_) / dt;
    }
    first_run_ = false;
    last_error_ = error;

    double output = p + i + d;

    // Clamp and back-calculate integral to prevent windup
    if (output > output_max_) {
        output = output_max_;
        if (ki_ != 0.0) integral_ = (output - p - d) / ki_;
    } else if (output < output_min_) {
        output = output_min_;
        if (ki_ != 0.0) integral_ = (output - p - d) / ki_;
    }

    return output;
}
