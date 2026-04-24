#include <iostream>

// OpenCV is optional. If headers are not found, the binary still compiles
// but prints a diagnostic message at runtime.
#if defined(__has_include) && __has_include(<opencv2/opencv.hpp>)
#include <opencv2/opencv.hpp>
#define HAS_OPENCV 1
#else
#define HAS_OPENCV 0
#endif

int main()
{
#if !HAS_OPENCV
    std::cerr << "OpenCV not found. Install libopencv-dev and rebuild to enable video capture.\n";
    return 1;
#else
    // Camera index 0 = default camera; use 1 or 2 for external USB cameras.
    cv::VideoCapture cap(0);

    if (!cap.isOpened())
    {
        std::cerr << "Error: Could not open camera.\n";
        return -1;
    }

    cv::Mat frame;
    while (true)
    {
        cap >> frame;
        if (frame.empty())
            break;

        cv::imshow("Camera Feed", frame);

        // Press any key to quit.
        if (cv::waitKey(30) >= 0)
            break;
    }

    return 0;
#endif
}
