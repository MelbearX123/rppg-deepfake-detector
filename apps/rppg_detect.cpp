#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: rppg_detect <video>\n";
        return 1;
    }

    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
        std::cerr << "could not open " << argv[1] << "\n";
        return 1;
    }

    std::cout << "frames: " << cap.get(cv::CAP_PROP_FRAME_COUNT)
              << "  fps: " << cap.get(cv::CAP_PROP_FPS)
              << "  size: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";

    // Decode every frame to prove the codec path works end to end.
    cv::Mat frame;
    int decoded = 0;
    while (cap.read(frame)) {
        ++decoded;
    }
    std::cout << "decoded: " << decoded << " frames\n";
    return 0;
}
