#pragma once

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cstdint>
#include <string>

namespace rppg {

struct VideoInfo {
    std::string path;
    std::string codec;  //four-character code, e.g. "h264", "avc1"
    int width = 0;
    int height = 0;
    double nominal_fps = 0.0;
    int nominal_frame_count = 0;
    std::uintmax_t file_bytes = 0;
};

struct Frame {
    cv::Mat image;        
    double time_sec = 0;  
    int index = -1;       
};


class VideoReader {
public:
    explicit VideoReader(const std::string& path);

    const VideoInfo& info() const { return info_; }

    bool read(Frame& frame);

private:
    cv::VideoCapture capture_;
    VideoInfo info_;
    int next_index_ = 0;
};

}  // namespace rppg
