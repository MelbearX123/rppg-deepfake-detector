#include "rppg/video.hpp"

#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace rppg {

namespace {

std::string decode_fourcc(int fourcc) {
    if (fourcc == 0) {
        return "raw";
    }
    std::string code;
    for (int shift = 0; shift < 32; shift += 8) {
        const char c = static_cast<char>((fourcc >> shift) & 0xFF);
        if (!std::isprint(static_cast<unsigned char>(c))) {
            return "unknown";
        }
        code += c;
    }
    return code;
}

}  // namespace

VideoReader::VideoReader(const std::string& path) {
    if (!capture_.open(path, cv::CAP_FFMPEG)) {
        throw std::runtime_error("could not open video: " + path);
    }

    info_.path = path;
    info_.codec = decode_fourcc(static_cast<int>(capture_.get(cv::CAP_PROP_FOURCC)));
    info_.width = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
    info_.height = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
    info_.nominal_fps = capture_.get(cv::CAP_PROP_FPS);
    info_.file_bytes = std::filesystem::file_size(path);
}

bool VideoReader::read(Frame& frame) {
    if (!capture_.read(frame.image)) {
        return false;
    }
    frame.time_sec = capture_.get(cv::CAP_PROP_POS_MSEC) / 1000.0;
    return true;
}

}  // namespace rppg
