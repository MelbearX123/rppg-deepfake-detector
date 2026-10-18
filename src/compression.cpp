#include "rppg/compression.hpp"

#include "rppg/limits.hpp"

namespace rppg {

double bits_per_pixel(std::uintmax_t file_bytes, int width, int height, std::size_t frame_count) {
    if (file_bytes == 0 || width <= 0 || height <= 0 || frame_count == 0) {
        return 0.0;
    }
    const double total_pixels =
        static_cast<double>(width) * height * static_cast<double>(frame_count);
    return static_cast<double>(file_bytes) * 8.0 / total_pixels;
}

bool is_heavily_compressed(double bits_per_pixel) {
    return bits_per_pixel < limits::kHeavyCompressionBpp;
}

}  // namespace rppg
