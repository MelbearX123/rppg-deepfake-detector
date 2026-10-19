#include "rppg/video/compression.hpp"

#include <gtest/gtest.h>

#include <cstdint>

TEST(Compression, UncompressedVideoIs24BitsPerPixel) {
    // Raw 8-bit BGR: 3 bytes per pixel = 24 bits per pixel.
    const int width = 640;
    const int height = 480;
    const std::size_t frames = 30;
    const auto bytes = static_cast<std::uintmax_t>(width) * height * 3 * frames;
    EXPECT_NEAR(rppg::bits_per_pixel(bytes, width, height, frames), 24.0, 1e-9);
}

TEST(Compression, InvalidInputGivesZero) {
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(0, 640, 480, 30), 0.0);
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(1000, 0, 480, 30), 0.0);
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(1000, 640, 0, 30), 0.0);
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(1000, 640, 480, 0), 0.0);
}

TEST(Compression, HeavyCompressionThreshold) {
    EXPECT_TRUE(rppg::is_heavily_compressed(0.03));
    EXPECT_FALSE(rppg::is_heavily_compressed(0.5));
}
