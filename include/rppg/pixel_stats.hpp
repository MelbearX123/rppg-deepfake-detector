#pragma once

#include <optional>
#include <vector>

namespace rppg {

struct Color {
    double b = 0.0;
    double g = 0.0;
    double r = 0.0;
};

double luma(const Color& color);

std::optional<Color> trimmed_mean(std::vector<Color> pixels, double trim_fraction);

}  // namespace rppg
