#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>          // CSRT, KCF
#include <vector>

struct Detection {
    int id = -1;                         // -1 when in pure detect mode
    cv::Rect bbox;
    cv::Point centroid;
};

class MotionTracker {
public:
    enum class Mode { Detect, Track };
    enum class TrackerType { MIL, NANO, VIT };

    MotionTracker(
        int history = 500,
        double varThreshold = 40.0,
        bool detectShadows = true,
        int minArea = 500,
        int maxArea = 50000,
        int blurKsize = 5,
        int morphKsize = 5,
        TrackerType trackerType = TrackerType::MIL,
        Mode mode = Mode::Detect,
        int maxTrackers = 10
        );

    // Main processing function
    // Returns annotated frame + list of detections
    std::pair<cv::Mat, std::vector<Detection>> process(const cv::Mat& frame, bool draw = true);

    // Pure detection (returns bounding boxes only)
    std::vector<cv::Rect> detect(const cv::Mat& frame);

    void reset();

    // Getters / setters
    void setMode(Mode mode) { mode_ = mode; }
    Mode mode() const { return mode_; }

private:
    // Background subtraction
    cv::Ptr<cv::BackgroundSubtractorMOG2> bgSubtractor_;

    // Parameters
    int minArea_;
    int maxArea_;
    int blurKsize_;
    int morphKsize_;
    Mode mode_;
    TrackerType trackerType_;
    int maxTrackers_;

    cv::Mat kernel_;

    // Tracking state
    std::vector<cv::Ptr<cv::Tracker>> trackers_;
    std::vector<cv::Rect> trackerBboxes_;
    std::vector<int> objectIds_;
    int nextId_ = 0;

    // Helpers
    cv::Ptr<cv::Tracker> createTracker() const;
    void updateTrackers(const cv::Mat& frame);
    void addTrackers(const cv::Mat& frame, const std::vector<cv::Rect>& bboxes);
    static double iou(const cv::Rect& a, const cv::Rect& b);

    // Drawing colors
    static const std::vector<cv::Scalar> colors_;
};