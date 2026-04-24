#include <iostream>

#if defined(__has_include)
#if __has_include(<opencv2/opencv.hpp>)
#include <opencv2/opencv.hpp>
#define HAS_OPENCV 1
#else
#define HAS_OPENCV 0
#endif
#else
#define HAS_OPENCV 0
#endif

// basic video capture and display using OpenCV.
int main()
{
#if !HAS_OPENCV
    std::cerr << "OpenCV headers not found. Install OpenCV development packages to enable video capture." << std::endl;
    return 1;
#else
    // 0 is the default camera ID. Use 1 or 2 for external USB cameras.
    cv::VideoCapture cap(0);

    if (!cap.isOpened())
    {
        std::cerr << "Error: Could not open camera." << std::endl;
        return -1;
    }

    cv::Mat frame;
    while (true)
    {
        cap >> frame; // Capture a new frame
        if (frame.empty())
            break;

        cv::imshow("Camera Feed", frame); // Display the frame

        // Wait for 30ms; exit if any key is pressed
        if (cv::waitKey(30) >= 0)
            break;
    }
    return 0;
#endif
}
