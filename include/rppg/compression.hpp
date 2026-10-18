#pragma once

#include <cstdint>

namespace rppg {

double bits_per_pixel(std::uintmax_t file_bytes, double duration_sec,
                      int width, int height, double fps);
                      
bool is_heavily_compressed(double bits_per_pixel);

}  // namespace rppg
