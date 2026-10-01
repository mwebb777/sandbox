# Raspberry Pi 5 Two-Axis Camera Pan-Tilt Tracker (C++)

A C++ application for **Raspberry Pi 5** that performs real-time video object tracking and controls a two-axis (pan/tilt) camera mount using servo motors.

## Features

- Real-time camera capture via OpenCV (supports Raspberry Pi Camera Module 3 via libcamera or USB webcams)
- Color-based object tracking (default: red / bright objects) with contour detection
- Optional face tracking using Haar cascades (if cascade file is present)
- **Kalman filter** (constant-velocity model) for smooth centroid estimates and short-term prediction during occlusion
- Dual PID controllers for smooth pan and tilt motion
- Hardware PWM servo control via Linux kernel sysfs interface (compatible with Raspberry Pi 5 RP1)
- Manual override via keyboard
- Configurable tracking mode, PID gains, and servo limits
- Frame rate display and status overlay (LOCK / PREDICT / SEARCH)

## Hardware Requirements

| Component              | Recommendation                          | Notes |
|------------------------|-----------------------------------------|-------|
| Board                  | Raspberry Pi 5 (2/4/8 GB)              | Required |
| Camera                 | Pi Camera Module 3 or USB UVC camera   | `/dev/video0` |
| Pan Servo              | SG90 / MG996R or continuous rotation   | 4.8–6 V |
| Tilt Servo             | SG90 / MG996R                          | 4.8–6 V |
| Power                  | Separate 5 V / 2 A+ supply for servos  | **Do not power servos from Pi 5 V** |
| Connections            | GPIO 12 (PWM0) → Pan, GPIO 13 (PWM1) → Tilt | See wiring below |

### Wiring (BCM numbering)

```
Servo Power (red)  → External 5 V
Servo GND (brown)  → Common GND with Pi
Pan Signal (orange)→ GPIO 12 (Pin 32)
Tilt Signal        → GPIO 13 (Pin 33)
```

**Important:** Enable PWM in `/boot/firmware/config.txt` (or `/boot/config.txt`):

```
dtoverlay=pwm-2chan
```

Reboot after editing. This exposes `/sys/class/pwm/pwmchip0` (or `pwmchip2` on some kernels).

## Software Dependencies

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config
sudo apt install -y libopencv-dev libopencv-contrib-dev
# Optional but recommended for Pi Camera Module 3
sudo apt install -y libcamera-dev libcamera-apps
```

No pigpio / wiringPi — they are incompatible with the RP1 southbridge on Pi 5. This project uses the kernel PWM sysfs interface.

## Build

```bash
cd pan_tilt_tracker
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
# Give PWM access (or run as root / add udev rule)
sudo ./pan_tilt_tracker

# Or with camera index and tracking mode
sudo ./pan_tilt_tracker --camera 0 --mode color
```

### Command-line options

```
--camera N          Camera index (default 0)
--mode color|face   Tracking mode (default color)
--cascade PATH      Path to Haar cascade XML (for face mode)
--width W --height H  Capture resolution (default 640x480)
--help
```

### Keyboard controls (while running)

| Key | Action                |
|-----|-----------------------|
| q / ESC | Quit               |
| c   | Toggle tracking mode  |
| r   | Reset servos to center|
| +/- | Adjust tracking sensitivity |
| m   | Toggle manual mode    |
| arrows | Manual pan/tilt (in manual mode) |

## Configuration

Edit `config/defaults.hpp` or the constants at the top of the source files for:

- PID gains (Kp, Ki, Kd)
- Servo pulse limits (µs)
- Color HSV thresholds
- Deadband and max speed

## Architecture

```
main.cpp
 ├── ObjectTracker   – OpenCV capture + detection + Kalman filter
 │                      State: [x, y, vx, vy]
 │                      Predicts for up to ~0.5 s when detection is lost
 ├── PIDController    – Independent pan & tilt PID loops
 └── ServoController – sysfs PWM duty-cycle control
```

Control loop runs at ~30 Hz (limited by camera).  
Filtered (or predicted) pixel error from frame center → PID → angle → PWM pulse width.

### Kalman Filter Details

- Model: constant-velocity (4-state, 2-measurement)
- On first detection the state is hard-initialized to avoid lag
- While measurements arrive → `predict()` + `correct()`
- When detection is briefly lost → pure `predict()` (status shows **PREDICT**)
- After `max_lost_frames` (default 15) the track is dropped and the system returns to **SEARCH**
- Visual feedback: green dot = raw measurement, cyan circle + velocity arrow = filtered/predicted position

Tune process / measurement noise in `ObjectTracker::initKalman()` if the track is too laggy or too jumpy.

## Safety Notes

1. Always use a separate power supply for the servos.
2. Limit servo travel in software to avoid mechanical damage.
3. Start with low PID gains to prevent oscillation.
4. The application clamps angles to safe ranges (default 10°–170°).

## Extending

- Replace color tracker with YOLO / MediaPipe for multi-object or person tracking.
- Increase state dimension (acceleration) or switch to an EKF/UKF for non-linear motion.
- Drive continuous-rotation servos or steppers via different drivers.
- Integrate with ROS 2 for higher-level autonomy.

## License

MIT – free for military, educational, and commercial use.
