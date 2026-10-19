#include "rppg/pixel_stats.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace rppg {

namespace {

//Standard brightness weights (ITU-R BT.601): eyes are most sensitive to green.
constexpr double kBlueWeight = 0.114;
constexpr double kGreenWeight = 0.587;
constexpr double kRedWeight = 0.299;

}  // namespace

double luma(const Color& color) {
    return kBlueWeight * color.b + kGreenWeight * color.g + kRedWeight * color.r;
}

std::optional<Color> trimmed_mean(std::vector<Color> pixels, double trim_fraction) {
    if (pixels.empty()) {
        return std::nullopt;
    }

    std::sort(pixels.begin(), pixels.end(),
              [](const Color& a, const Color& b) { return luma(a) < luma(b); });

    std::size_t drop = static_cast<std::size_t>(std::floor(pixels.size() * trim_fraction));
    if (2 * drop >= pixels.size()) {
        drop = 0;
    }
    const std::size_t first_kept = drop;
    const std::size_t last_kept = pixels.size() - drop;

    Color sum;
    for (std::size_t i = first_kept; i < last_kept; ++i) {
        sum.b += pixels[i].b;
        sum.g += pixels[i].g;
        sum.r += pixels[i].r;
    }
    const auto kept = static_cast<double>(last_kept - first_kept);
    return Color{sum.b / kept, sum.g / kept, sum.r / kept};
}

}  // namespace rppg
