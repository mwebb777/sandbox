#include <opencv2/opencv.hpp>

#include <cstdio>
#include <iostream>
#include <vector>

int main() {
    constexpr int width = 640;
    constexpr int height = 480;
    const std::size_t frameSize = width * height * 3 / 2; // YUV420

    // rpicam-vid writes raw YUV420 frames to stdout.
    const char* command =
        "rpicam-vid -t 0 --nopreview "
        "--width 640 --height 480 --framerate 30 "
        "--codec yuv420 -o -";

    FILE* camera = popen(command, "r");
    if (!camera) {
        std::cerr << "Could not start rpicam-vid\n";
        return 1;
    }

    std::vector<unsigned char> buffer(frameSize);

    cv::namedWindow("Camera", cv::WINDOW_AUTOSIZE);

    while (true) {
        // Read one complete YUV420 frame.
        const std::size_t bytesRead =
            fread(buffer.data(), 1, frameSize, camera);

        if (bytesRead != frameSize) {
            std::cerr << "Could not read a complete frame\n";
            break;
        }

        // YUV420 (I420) to OpenCV's usual BGR format.
        cv::Mat yuv(height * 3 / 2, width, CV_8UC1, buffer.data());
        cv::Mat bgr;
        cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_I420);

        cv::imshow("Camera", bgr);

        if (cv::waitKey(1) == 27) { // Esc to quit
            break;
        }
    }

    pclose(camera);
    cv::destroyAllWindows();
    return 0;
}
