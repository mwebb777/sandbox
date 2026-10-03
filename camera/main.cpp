#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    // Open the default camera (device index 0)
    cv::VideoCapture cap(0);

    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open webcam." << std::endl;
        return -1;
    }

    // Optional: set resolution (comment out if you want the camera default)
    // cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);

    cv::Mat frame;
    const std::string windowName = "Webcam Viewer";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    std::cout << "Press ESC to quit." << std::endl;

    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Error: Failed to capture frame." << std::endl;
            break;
        }

        cv::imshow(windowName, frame);

        // Wait 1 ms and check for ESC key (ASCII 27)
        if (cv::waitKey(1) == 27) {
            break;
        }
    }

    // Cleanup
    cap.release();
    cv::destroyAllWindows();

    return 0;
}