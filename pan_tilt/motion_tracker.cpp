#include "motion_tracker.h"
#include <algorithm>
#include <opencv2/imgproc/imgproc.hpp>

const std::vector<cv::Scalar> MotionTracker::colors_ = {
    {0, 255, 0}, {255, 0, 0}, {0, 0, 255},
    {255, 255, 0}, {255, 0, 255}, {0, 255, 255},
    {128, 255, 0}, {255, 128, 0}
};

MotionTracker::MotionTracker(
    int history, double varThreshold, bool detectShadows,
    int minArea, int maxArea, int blurKsize, int morphKsize,
    TrackerType trackerType, Mode mode, int maxTrackers)
    : minArea_(minArea)
    , maxArea_(maxArea)
    , blurKsize_(blurKsize)
    , morphKsize_(morphKsize)
    , mode_(mode)
    , trackerType_(trackerType)
    , maxTrackers_(maxTrackers)
{
    bgSubtractor_ = cv::createBackgroundSubtractorMOG2(history, varThreshold, detectShadows);
    kernel_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, {morphKsize_, morphKsize_});
}

cv::Ptr<cv::Tracker> MotionTracker::createTracker() const {
    switch (trackerType_) {
    case TrackerType::MIL:   return cv::TrackerMIL::create();
    case TrackerType::NANO:  return cv::TrackerNano::create();
    case TrackerType::VIT:   return cv::TrackerVit::create();
    default:                 return cv::TrackerMIL::create();
    }
}

std::vector<cv::Rect> MotionTracker::detect(const cv::Mat& frame) {
    cv::Mat processed = frame.clone();

    if (blurKsize_ > 0) {
        int k = blurKsize_ | 1;          // ensure odd
        cv::GaussianBlur(processed, processed, {k, k}, 0);
    }

    cv::Mat fgMask;
    bgSubtractor_->apply(processed, fgMask);

    // Remove shadows (value 127) and weak detections
    cv::threshold(fgMask, fgMask, 200, 255, cv::THRESH_BINARY);

    // Morphological cleaning
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_OPEN,  kernel_);
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_CLOSE, kernel_);
    cv::dilate(fgMask, fgMask, kernel_, {-1, -1}, 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fgMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<cv::Rect> bboxes;
    // for (const auto& cnt : contours) {
    //     double area = cv::contourArea(cnt);
    //     if (area > minArea_ && area < maxArea_) {
    //         bboxes.push_back(cv::boundingRect(cnt));
    //     }
    // }
    return bboxes;
}

void MotionTracker::updateTrackers(const cv::Mat& frame) {
    std::vector<cv::Ptr<cv::Tracker>> aliveTrackers;
    std::vector<cv::Rect> aliveBboxes;
    std::vector<int> aliveIds;

    for (size_t i = 0; i < trackers_.size(); ++i) {
        cv::Rect box;
        bool ok = trackers_[i]->update(frame, box);
        if (ok) {
            aliveTrackers.push_back(trackers_[i]);
            aliveBboxes.push_back(box);
            aliveIds.push_back(objectIds_[i]);
        }
    }

    trackers_      = std::move(aliveTrackers);
    trackerBboxes_ = std::move(aliveBboxes);
    objectIds_     = std::move(aliveIds);
}

void MotionTracker::addTrackers(const cv::Mat& frame, const std::vector<cv::Rect>& bboxes) {
    for (const auto& bbox : bboxes) {
        if (static_cast<int>(trackers_.size()) >= maxTrackers_)
            break;

        auto tracker = createTracker();
        tracker->init(frame, bbox);
        trackers_.push_back(tracker);
        objectIds_.push_back(nextId_++);
        trackerBboxes_.push_back(bbox);
    }
}

double MotionTracker::iou(const cv::Rect& a, const cv::Rect& b) {
    int xA = std::max(a.x, b.x);
    int yA = std::max(a.y, b.y);
    int xB = std::min(a.x + a.width,  b.x + b.width);
    int yB = std::min(a.y + a.height, b.y + b.height);

    int interArea = std::max(0, xB - xA) * std::max(0, yB - yA);
    if (interArea == 0) return 0.0;

    double unionArea = a.area() + b.area() - interArea;
    return interArea / unionArea;
}

std::pair<cv::Mat, std::vector<Detection>>
MotionTracker::process(const cv::Mat& frame, bool draw) {
    cv::Mat output = frame.clone();
    std::vector<Detection> detections;

    if (mode_ == Mode::Detect) {
        auto bboxes = detect(frame);

        for (size_t i = 0; i < bboxes.size(); ++i) {
            const auto& box = bboxes[i];
            Detection det;
            det.id = -1;
            det.bbox = box;
            det.centroid = {box.x + box.width / 2, box.y + box.height / 2};
            detections.push_back(det);

            if (draw) {
                const auto& color = colors_[i % colors_.size()];
                cv::rectangle(output, box, color, 2);
                cv::circle(output, det.centroid, 4, color, -1);
            }
        }
    }
    else { // Mode::Track
        // 1. Update existing trackers
        if (!trackers_.empty()) {
            updateTrackers(frame);
        }

        // 2. Detect new objects if we still have free slots
        if (static_cast<int>(trackers_.size()) < maxTrackers_) {
            auto newBboxes = detect(frame);

            // Filter out boxes that heavily overlap existing trackers
            std::vector<cv::Rect> filtered;
            for (const auto& nb : newBboxes) {
                bool overlaps = false;
                for (const auto& eb : trackerBboxes_) {
                    if (iou(nb, eb) > 0.3) {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps) {
                    filtered.push_back(nb);
                }
            }
            addTrackers(frame, filtered);
        }

        // 3. Build detection list from current trackers
        for (size_t i = 0; i < trackers_.size(); ++i) {
            Detection det;
            det.id = objectIds_[i];
            det.bbox = trackerBboxes_[i];
            det.centroid = {
                det.bbox.x + det.bbox.width / 2,
                det.bbox.y + det.bbox.height / 2
            };
            detections.push_back(det);

            if (draw) {
                const auto& color = colors_[det.id % colors_.size()];
                cv::rectangle(output, det.bbox, color, 2);
                cv::putText(output, "ID " + std::to_string(det.id),
                            {det.bbox.x, det.bbox.y - 8},
                            cv::FONT_HERSHEY_SIMPLEX, 0.55, color, 2);
                cv::circle(output, det.centroid, 4, color, -1);
            }
        }
    }

    return {output, detections};
}

void MotionTracker::reset() {
    trackers_.clear();
    trackerBboxes_.clear();
    objectIds_.clear();
    nextId_ = 0;

    // Recreate background subtractor to clear history
    bgSubtractor_ = cv::createBackgroundSubtractorMOG2(500, 40.0, true);
}