#pragma once

#include <cstddef>
#include <cstdint>

namespace rppg {

double bits_per_pixel(std::uintmax_t file_bytes, int width, int height, std::size_t frame_count);

bool is_heavily_compressed(double bits_per_pixel);

}  // namespace rppg
