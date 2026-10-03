#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <string>
#include <optional>
#include <utility>

enum class TrackingMode {
    Color,
    Face
};

/**
 * Captures video and returns the centroid of the tracked object
 * relative to the frame center (error in pixels).
 *
 * Uses an OpenCV Kalman filter (constant-velocity model) to:
 *  - Smooth noisy detections
 *  - Predict position during short occlusions / lost frames
 *  - Provide a continuous track for the PID loops
 *
 * Color mode: HSV thresholding + largest contour.
 * Face mode: Haar cascade (requires cascade XML file).
 */
class ObjectTracker {
public:
    ObjectTracker(int camera_index = 0,
                  int width = 640, int height = 480,
                  TrackingMode mode = TrackingMode::Color);

    ~ObjectTracker();

    bool open();
    void close();
    bool isOpened() const;

    void setMode(TrackingMode mode);
    TrackingMode getMode() const { return mode_; }

    /**
     * Load Haar cascade for face mode.
     * Returns false if file cannot be loaded.
     */
    bool loadCascade(const std::string& path);

    /**
     * Grab a frame, run detection, apply Kalman filter, and compute
     * error from center.
     *
     * @param frame_out  Optional output of annotated frame
     * @return           {error_x, error_y} in pixels (positive = object right/down of center)
     *                   std::nullopt only after the track has been lost for
     *                   more than max_lost_frames_
     */
    std::optional<std::pair<double, double>> process(cv::Mat* frame_out = nullptr);

    /**
     * Adjust HSV lower/upper bounds for color tracking (live tuning).
     */
    void setHSVRange(const cv::Scalar& lower, const cv::Scalar& upper);

    /** Reset Kalman state (call after long occlusion or mode change). */
    void resetTrack();

    /** Maximum consecutive frames without a measurement before declaring lost. */
    void setMaxLostFrames(int n) { max_lost_frames_ = n; }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    double getFps() const { return fps_; }
    bool isTracking() const { return tracking_active_; }
    int getLostFrames() const { return lost_frames_; }

private:
    void initKalman();
    std::optional<cv::Point2f> detectColor(const cv::Mat& frame, cv::Mat& debug);
    std::optional<cv::Point2f> detectFace(const cv::Mat& frame, cv::Mat& debug);

    cv::VideoCapture cap_;
    int camera_index_;
    int width_, height_;
    TrackingMode mode_;

    // Color tracking defaults (bright red / orange-ish)
    cv::Scalar hsv_lower_{0, 120, 70};
    cv::Scalar hsv_upper_{10, 255, 255};
    // Second range for red wrap-around
    cv::Scalar hsv_lower2_{170, 120, 70};
    cv::Scalar hsv_upper2_{180, 255, 255};

   // cv::CascadeClassifier face_cascade_;
    bool cascade_loaded_ = false;

    // --- Kalman filter (constant velocity model) ---
    // State: [x, y, vx, vy]^T
    // Measurement: [x, y]^T
    cv::KalmanFilter kf_;
    bool kalman_initialized_ = false;
    bool tracking_active_ = false;
    int lost_frames_ = 0;
    int max_lost_frames_ = 15;          // ~0.5 s at 30 fps
    cv::Point2f last_filtered_{0.f, 0.f};

    double fps_ = 0.0;
    int64_t last_tick_ = 0;
    int frame_count_ = 0;
};
