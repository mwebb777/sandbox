#include "object_tracker.h"
#include <iostream>
#include <chrono>
#include <cmath>

ObjectTracker::ObjectTracker(int camera_index, int width, int height, TrackingMode mode)
    : camera_index_(camera_index), width_(width), height_(height), mode_(mode)
{
    initKalman();
}

ObjectTracker::~ObjectTracker() {
    close();
}

void ObjectTracker::initKalman() {
    // 4 state variables (x, y, vx, vy), 2 measurements (x, y), 0 control inputs
    kf_.init(4, 2, 0, CV_32F);

    // Transition matrix A (constant velocity, dt ≈ 1 frame; we scale velocity later if needed)
    // [ 1 0 1 0 ]
    // [ 0 1 0 1 ]
    // [ 0 0 1 0 ]
    // [ 0 0 0 1 ]
    kf_.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1);

    // Measurement matrix H
    // [ 1 0 0 0 ]
    // [ 0 1 0 0 ]
    kf_.measurementMatrix = (cv::Mat_<float>(2, 4) <<
        1, 0, 0, 0,
        0, 1, 0, 0);

    // Process noise covariance Q – moderate process noise on velocity
    // Higher values → filter trusts model less, follows measurements more
    cv::setIdentity(kf_.processNoiseCov, cv::Scalar::all(1e-2));
    kf_.processNoiseCov.at<float>(2, 2) = 1e-1;  // vx
    kf_.processNoiseCov.at<float>(3, 3) = 1e-1;  // vy

    // Measurement noise covariance R – detector is noisy
    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(5e-1));

    // Posterior error covariance
    cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(1));

    // Initial state
    kf_.statePost = cv::Mat::zeros(4, 1, CV_32F);

    kalman_initialized_ = true;
    tracking_active_ = false;
    lost_frames_ = 0;
}

void ObjectTracker::resetTrack() {
    kf_.statePost = cv::Mat::zeros(4, 1, CV_32F);
    cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(1));
    tracking_active_ = false;
    lost_frames_ = 0;
    last_filtered_ = {0.f, 0.f};
}

bool ObjectTracker::open() {
    // Prefer V4L2 backend; on Pi Camera with libcamera this still works via the
    // compatibility layer or after installing libcamera-apps.
    cap_.open(camera_index_, cv::CAP_V4L2);
    if (!cap_.isOpened()) {
        // Fallback
        cap_.open(camera_index_);
    }
    if (!cap_.isOpened()) {
        std::cerr << "ERROR: Cannot open camera " << camera_index_ << std::endl;
        return false;
    }

    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap_.set(cv::CAP_PROP_FPS, 30);
    // Reduce buffering for lower latency
    cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);

    // Read actual resolution
    width_  = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));

    last_tick_ = cv::getTickCount();
    resetTrack();
    return true;
}

void ObjectTracker::close() {
    if (cap_.isOpened()) {
        cap_.release();
    }
}

bool ObjectTracker::isOpened() const {
    return cap_.isOpened();
}

void ObjectTracker::setMode(TrackingMode mode) {
    mode_ = mode;
    resetTrack();   // clear track when switching detectors
}

bool ObjectTracker::loadCascade(const std::string& path) {
    // cascade_loaded_ = face_cascade_.load(path);
    // if (!cascade_loaded_) {
    //     std::cerr << "WARNING: Failed to load cascade: " << path << std::endl;
    // }
    // return cascade_loaded_;
    return false;
}

void ObjectTracker::setHSVRange(const cv::Scalar& lower, const cv::Scalar& upper) {
    hsv_lower_ = lower;
    hsv_upper_ = upper;
}

std::optional<cv::Point2f> ObjectTracker::detectColor(const cv::Mat& frame, cv::Mat& debug) {
    cv::Mat hsv, mask1, mask2, mask;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    cv::inRange(hsv, hsv_lower_, hsv_upper_, mask1);
    cv::inRange(hsv, hsv_lower2_, hsv_upper2_, mask2);
    mask = mask1 | mask2;

    // Morphological clean-up
    cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);
    cv::dilate(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return std::nullopt;

    // // Largest contour by area
    // auto largest = std::max_element(contours.begin(), contours.end(),
    //     [](const auto& a, const auto& b) {
    //         return cv::contourArea(a) < cv::contourArea(b);
    //     });

    // double area = cv::contourArea(*largest);
    // if (area < 400.0) return std::nullopt;  // noise rejection

    // cv::Moments m = cv::moments(*largest);
    // if (m.m00 == 0) return std::nullopt;

    // cv::Point2f centroid(static_cast<float>(m.m10 / m.m00),
    //                      static_cast<float>(m.m01 / m.m00));

    cv::Point2f centroid(static_cast<float>(0.0f),
                          static_cast<float>(0.0f));

    // Draw for debug
    // cv::drawContours(debug, contours, static_cast<int>(largest - contours.begin()),
    //                  cv::Scalar(0, 255, 0), 2);
    // cv::circle(debug, centroid, 5, cv::Scalar(0, 255, 0), -1);  // raw measurement (green)

    return centroid;
}

std::optional<cv::Point2f> ObjectTracker::detectFace(const cv::Mat& frame, cv::Mat& debug) {
    if (!cascade_loaded_) return std::nullopt;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
//    face_cascade_.detectMultiScale(gray, faces, 1.1, 4, 0, cv::Size(60, 60));

    if (faces.empty()) return std::nullopt;

    // Largest face
    auto largest = std::max_element(faces.begin(), faces.end(),
        [](const cv::Rect& a, const cv::Rect& b) {
            return a.area() < b.area();
        });

    cv::Point2f centroid(largest->x + largest->width  * 0.5f,
                         largest->y + largest->height * 0.5f);

    cv::rectangle(debug, *largest, cv::Scalar(255, 0, 0), 2);
    cv::circle(debug, centroid, 5, cv::Scalar(0, 255, 0), -1);  // raw measurement

    return centroid;
}

std::optional<std::pair<double, double>> ObjectTracker::process(cv::Mat* frame_out) {
    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty()) {
        return std::nullopt;
    }

    // FPS calculation
    frame_count_++;
    int64_t now = cv::getTickCount();
    double elapsed = (now - last_tick_) / cv::getTickFrequency();
    if (elapsed >= 1.0) {
        fps_ = frame_count_ / elapsed;
        frame_count_ = 0;
        last_tick_ = now;
    }

    cv::Mat debug = frame.clone();
    std::optional<cv::Point2f> measurement;

    if (mode_ == TrackingMode::Color) {
        measurement = detectColor(frame, debug);
    } else {
        measurement = detectFace(frame, debug);
    }

    // -------------------- Kalman predict / correct --------------------
    cv::Point2f filtered;
    bool have_estimate = false;

    // Always predict first
    cv::Mat prediction = kf_.predict();
    cv::Point2f predicted(prediction.at<float>(0), prediction.at<float>(1));

    if (measurement) {
        // We have a new observation → correct
        cv::Mat meas(2, 1, CV_32F);
        meas.at<float>(0) = measurement->x;
        meas.at<float>(1) = measurement->y;

        if (!tracking_active_) {
            // First detection: hard-initialize state so filter doesn't lag
            kf_.statePost.at<float>(0) = measurement->x;
            kf_.statePost.at<float>(1) = measurement->y;
            kf_.statePost.at<float>(2) = 0.f;  // vx
            kf_.statePost.at<float>(3) = 0.f;  // vy
            cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(1));
            tracking_active_ = true;
        }

        cv::Mat estimated = kf_.correct(meas);
        filtered.x = estimated.at<float>(0);
        filtered.y = estimated.at<float>(1);
        lost_frames_ = 0;
        have_estimate = true;
    } else if (tracking_active_) {
        // No measurement – use pure prediction
        lost_frames_++;
        if (lost_frames_ <= max_lost_frames_) {
            filtered = predicted;
            have_estimate = true;
        } else {
            // Track lost for too long
            tracking_active_ = false;
            have_estimate = false;
        }
    }

    if (have_estimate) {
        last_filtered_ = filtered;

        // Draw filtered / predicted position (cyan)
        cv::circle(debug, filtered, 8, cv::Scalar(255, 255, 0), 2);
        // Small velocity vector
        float vx = kf_.statePost.at<float>(2);
        float vy = kf_.statePost.at<float>(3);
        cv::arrowedLine(debug, filtered,
                        filtered + cv::Point2f(vx * 5.f, vy * 5.f),
                        cv::Scalar(255, 200, 0), 2, cv::LINE_AA, 0, 0.3);
    }

    // Draw crosshair at frame center
    cv::Point frame_center(width_ / 2, height_ / 2);
    cv::drawMarker(debug, frame_center, cv::Scalar(255, 255, 0),
                   cv::MARKER_CROSS, 20, 2);

    // Status overlay
    std::string mode_str = (mode_ == TrackingMode::Color) ? "COLOR" : "FACE";
    std::string track_str = tracking_active_
        ? (lost_frames_ > 0 ? "PREDICT" : "LOCK")
        : "SEARCH";
    char status[128];
    std::snprintf(status, sizeof(status), "%s | %s | lost=%d  FPS: %d",
                  mode_str.c_str(), track_str.c_str(), lost_frames_,
                  static_cast<int>(fps_));
    cv::putText(debug, status, cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 255, 255), 2);

    if (frame_out) {
        *frame_out = debug;
    }

    if (!have_estimate) {
        return std::nullopt;
    }

    double err_x = filtered.x - frame_center.x;
    double err_y = filtered.y - frame_center.y;
    return std::make_pair(err_x, err_y);
}
