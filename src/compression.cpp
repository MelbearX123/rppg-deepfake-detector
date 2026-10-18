#include "rppg/compression.hpp"

#include "rppg/limits.hpp"

namespace rppg {

double bits_per_pixel(std::uintmax_t file_bytes, double duration_sec,
                      int width, int height, double fps) {
    if (file_bytes == 0 || duration_sec <= 0.0 || width <= 0 || height <= 0 || fps <= 0.0) {
        return 0.0;
    }
    const double bits_per_second = static_cast<double>(file_bytes) * 8.0 / duration_sec;
    const double pixels_per_second = static_cast<double>(width) * height * fps;
    return bits_per_second / pixels_per_second;
}

bool is_heavily_compressed(double bits_per_pixel) {
    return bits_per_pixel < limits::kHeavyCompressionBpp;
}

}  // namespace rppg
