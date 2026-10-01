/**
 * Raspberry Pi 5 – Two-Axis Camera Pan-Tilt Tracker
 *
 * Real-time object tracking + servo control using OpenCV and kernel PWM.
 *
 * Compile: see README.md / CMakeLists.txt
 * Run:     sudo ./pan_tilt_tracker
 */

#include "ObjectTracker.hpp"
#include "ServoController.hpp"
#include "PIDController.hpp"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <string>
#include <cmath>

// ---------------------------------------------------------------------------
// Configuration – tweak these for your hardware
// ---------------------------------------------------------------------------
namespace cfg {
    // PWM chip name after enabling dtoverlay=pwm-2chan
    // On most Pi 5 kernels this is "pwmchip0" or "pwmchip2". Check:
    //   ls /sys/class/pwm/
    const char* PWM_CHIP = "pwmchip0";

    // Channel 0 → GPIO 12 (pan), Channel 1 → GPIO 13 (tilt)
    const int   PAN_CHANNEL  = 0;
    const int   TILT_CHANNEL = 1;

    // Servo pulse limits (µs) and mechanical angle limits
    const int   SERVO_MIN_US = 600;
    const int   SERVO_MAX_US = 2400;
    const double SERVO_MIN_DEG = 10.0;   // keep away from hard stops
    const double SERVO_MAX_DEG = 170.0;

    // PID gains (start conservative – increase after testing)
    // Units: degrees of servo motion per pixel of error
    const double PAN_KP  = 0.06;
    const double PAN_KI  = 0.0008;
    const double PAN_KD  = 0.015;

    const double TILT_KP = 0.06;
    const double TILT_KI = 0.0008;
    const double TILT_KD = 0.015;

    // Deadband (pixels) – ignore tiny errors to reduce jitter
    const double DEADBAND = 12.0;

    // Maximum angular step per control cycle (degrees)
    const double MAX_STEP = 4.0;

    // Control loop target period
    const double LOOP_HZ = 30.0;
}

// ---------------------------------------------------------------------------
std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

void printUsage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [options]\n"
        "  --camera N          Camera index (default 0)\n"
        "  --mode color|face   Tracking mode (default color)\n"
        "  --cascade PATH      Haar cascade XML for face mode\n"
        "  --width W           Capture width  (default 640)\n"
        "  --height H          Capture height (default 480)\n"
        "  --pwmchip NAME      PWM chip name  (default pwmchip0)\n"
        "  --help\n";
}

int main(int argc, char** argv) {
    // --- Parse arguments ---------------------------------------------------
    int camera_index = 0;
    int width = 640, height = 480;
    TrackingMode mode = TrackingMode::Color;
    std::string cascade_path = "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml";
    std::string pwmchip = cfg::PWM_CHIP;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--camera" && i + 1 < argc) {
            camera_index = std::stoi(argv[++i]);
        } else if (arg == "--mode" && i + 1 < argc) {
            std::string m = argv[++i];
            mode = (m == "face") ? TrackingMode::Face : TrackingMode::Color;
        } else if (arg == "--cascade" && i + 1 < argc) {
            cascade_path = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg == "--pwmchip" && i + 1 < argc) {
            pwmchip = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // --- Signal handling ---------------------------------------------------
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "=== Raspberry Pi 5 Pan-Tilt Camera Tracker ===\n";

    // --- Initialize tracker ------------------------------------------------
    ObjectTracker tracker(camera_index, width, height, mode);
    if (!tracker.open()) {
        return 1;
    }
    if (mode == TrackingMode::Face) {
        if (!tracker.loadCascade(cascade_path)) {
            std::cerr << "Falling back to color mode.\n";
            tracker.setMode(TrackingMode::Color);
        }
    }
    std::cout << "Camera opened: " << tracker.getWidth() << "x" << tracker.getHeight() << "\n";

    // --- Initialize servos -------------------------------------------------
    ServoController pan (pwmchip, cfg::PAN_CHANNEL,
                         cfg::SERVO_MIN_US, cfg::SERVO_MAX_US,
                         cfg::SERVO_MIN_DEG, cfg::SERVO_MAX_DEG);
    ServoController tilt(pwmchip, cfg::TILT_CHANNEL,
                         cfg::SERVO_MIN_US, cfg::SERVO_MAX_US,
                         cfg::SERVO_MIN_DEG, cfg::SERVO_MAX_DEG);

    try {
        pan.enable();
        tilt.enable();
        pan.center();
        tilt.center();
        std::cout << "Servos enabled and centered.\n";
    } catch (const std::exception& e) {
        std::cerr << "Servo init failed: " << e.what() << "\n"
                  << "Make sure:\n"
                  << "  1. dtoverlay=pwm-2chan is in /boot/firmware/config.txt\n"
                  << "  2. You are running with sudo (or have udev rules)\n"
                  << "  3. Correct --pwmchip (check ls /sys/class/pwm/)\n";
        return 1;
    }

    // --- PID controllers ---------------------------------------------------
    // Setpoint is always 0 (we want the object centered → error = 0)
    PIDController pan_pid (cfg::PAN_KP,  cfg::PAN_KI,  cfg::PAN_KD, -cfg::MAX_STEP, cfg::MAX_STEP);
    PIDController tilt_pid(cfg::TILT_KP, cfg::TILT_KI, cfg::TILT_KD, -cfg::MAX_STEP, cfg::MAX_STEP);

    // --- Main control loop -------------------------------------------------
    bool manual_mode = false;
    auto prev_time = std::chrono::steady_clock::now();
    const double target_dt = 1.0 / cfg::LOOP_HZ;

    std::cout << "Tracking started. Press 'q' to quit, 'm' for manual, 'r' to recenter.\n";

    while (g_running) {
        auto t0 = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(t0 - prev_time).count();
        prev_time = t0;
        if (dt <= 0.0) dt = target_dt;

        cv::Mat frame;
        auto error = tracker.process(&frame);

        if (!manual_mode && error) {
            double err_x = error->first;
            double err_y = error->second;

            // Deadband
            if (std::abs(err_x) < cfg::DEADBAND) err_x = 0.0;
            if (std::abs(err_y) < cfg::DEADBAND) err_y = 0.0;

            // Note sign conventions:
            //   positive err_x (object right of center) → increase pan angle (or decrease depending on mount)
            //   Invert if your mechanical orientation is opposite.
            double pan_delta  = pan_pid.update(err_x, dt);   // measurement = error, setpoint = 0
            double tilt_delta = tilt_pid.update(err_y, dt);

            // Invert tilt if camera is mounted upside-down or linkage is reversed
            // tilt_delta = -tilt_delta;

            pan.moveBy(pan_delta);
            tilt.moveBy(tilt_delta);

            // Overlay status (Kalman may be predicting if lost_frames > 0)
            char buf[128];
            std::snprintf(buf, sizeof(buf), "err(%.0f,%.0f)  pan=%.1f  tilt=%.1f%s",
                          err_x, err_y, pan.getAngle(), tilt.getAngle(),
                          tracker.getLostFrames() > 0 ? "  [PRED]" : "");
            cv::putText(frame, buf, cv::Point(10, frame.rows - 20),
                        cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 0), 1);
        } else if (!error) {
            // Track fully lost – reset integrators so we don't jump on re-acquisition
            pan_pid.reset();
            tilt_pid.reset();
            cv::putText(frame, "NO TARGET", cv::Point(10, frame.rows - 20),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
        }

        if (manual_mode) {
            cv::putText(frame, "MANUAL MODE", cv::Point(10, frame.rows - 20),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 165, 255), 2);
        }

        if (!frame.empty()) {
            cv::imshow("Pan-Tilt Tracker", frame);
        }

        // Keyboard
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) {
            g_running = false;
        } else if (key == 'r') {
            pan.center();
            tilt.center();
            pan_pid.reset();
            tilt_pid.reset();
            tracker.resetTrack();          // clear Kalman state
            std::cout << "Servos recentered & track reset.\n";
        } else if (key == 'c') {
            TrackingMode new_mode = (tracker.getMode() == TrackingMode::Color)
                                    ? TrackingMode::Face : TrackingMode::Color;
            tracker.setMode(new_mode);     // also resets Kalman internally
            std::cout << "Switched to " << (new_mode == TrackingMode::Color ? "COLOR" : "FACE") << " mode\n";
        } else if (key == 'm') {
            manual_mode = !manual_mode;
            pan_pid.reset();
            tilt_pid.reset();
            if (manual_mode) tracker.resetTrack();
            std::cout << (manual_mode ? "Manual mode ON\n" : "Auto tracking ON\n");
        } else if (manual_mode) {
            // Arrow keys (OpenCV waitKey codes vary; common values)
            constexpr double MAN_STEP = 2.0;
            if (key == 81 || key == 2)  pan.moveBy(-MAN_STEP);   // left
            if (key == 83 || key == 3)  pan.moveBy( MAN_STEP);   // right
            if (key == 82 || key == 0)  tilt.moveBy(-MAN_STEP);  // up
            if (key == 84 || key == 1)  tilt.moveBy( MAN_STEP);  // down
        }

        // Rate limiting
        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        if (elapsed < target_dt) {
            std::this_thread::sleep_for(
                std::chrono::duration<double>(target_dt - elapsed));
        }
    }

    // --- Cleanup -----------------------------------------------------------
    std::cout << "\nShutting down...\n";
    pan.center();
    tilt.center();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    pan.disable();
    tilt.disable();
    tracker.close();
    cv::destroyAllWindows();

    std::cout << "Done.\n";
    return 0;
}
