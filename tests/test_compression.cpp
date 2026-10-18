#include "rppg/compression.hpp"

#include <gtest/gtest.h>

TEST(Compression, UncompressedVideoIs24BitsPerPixel) {
    // One second of raw 8-bit BGR: 3 bytes per pixel = 24 bits per pixel.
    const int width = 640;
    const int height = 480;
    const double fps = 30.0;
    const auto bytes = static_cast<std::uintmax_t>(width) * height * 3 * 30;
    EXPECT_NEAR(rppg::bits_per_pixel(bytes, 1.0, width, height, fps), 24.0, 1e-9);
}

TEST(Compression, InvalidInputGivesZero) {
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(0, 1.0, 640, 480, 30.0), 0.0);
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(1000, 0.0, 640, 480, 30.0), 0.0);
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(1000, 1.0, 0, 480, 30.0), 0.0);
    EXPECT_DOUBLE_EQ(rppg::bits_per_pixel(1000, 1.0, 640, 480, 0.0), 0.0);
}

TEST(Compression, HeavyCompressionThreshold) {
    EXPECT_TRUE(rppg::is_heavily_compressed(0.03));
    EXPECT_FALSE(rppg::is_heavily_compressed(0.5));
}
