#pragma once

/**
 * Simple discrete PID controller suitable for pan/tilt servo loops.
 * Uses positional form with anti-windup and output clamping.
 */
class PIDController {
public:
    PIDController(double kp = 0.08, double ki = 0.001, double kd = 0.02,
                  double output_min = -30.0, double output_max = 30.0);

    void setGains(double kp, double ki, double kd);
    void setOutputLimits(double min, double max);
    void setSetpoint(double setpoint);
    void reset();

    /**
     * Compute control output given current process variable (measurement).
     * @param measurement  Current value (e.g. pixel error or angle)
     * @param dt           Time step in seconds
     * @return             Control effort (degrees or velocity command)
     */
    double update(double measurement, double dt);

    double getError() const { return last_error_; }
    double getIntegral() const { return integral_; }

private:
    double kp_, ki_, kd_;
    double output_min_, output_max_;
    double setpoint_ = 0.0;
    double integral_ = 0.0;
    double last_error_ = 0.0;
    bool first_run_ = true;
};
