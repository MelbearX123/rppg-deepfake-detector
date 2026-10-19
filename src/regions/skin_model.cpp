#include "rppg/regions/skin_model.hpp"

#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rppg {

namespace {

constexpr double kMinTolerance = 3.0;

double median_absolute_deviation(const std::vector<double>& values, double center) {
    std::vector<double> distances;
    distances.reserve(values.size());
    for (double v : values) {
        distances.push_back(std::abs(v - center));
    }
    return median(distances);
}

}  // namespace

SkinModel fit_skin_model(const std::vector<YCrCb>& samples) {
    if (samples.empty()) {
        throw std::invalid_argument("fit_skin_model: no samples");
    }

    std::vector<double> y;
    std::vector<double> cr;
    std::vector<double> cb;
    y.reserve(samples.size());
    cr.reserve(samples.size());
    cb.reserve(samples.size());
    for (const YCrCb& pixel : samples) {
        y.push_back(pixel.y);
        cr.push_back(pixel.cr);
        cb.push_back(pixel.cb);
    }

    SkinModel model;
    model.cr_center = median(cr);
    model.cb_center = median(cb);
    model.cr_tolerance = std::max(
        kMinTolerance, limits::kSkinToleranceMads * median_absolute_deviation(cr, model.cr_center));
    model.cb_tolerance = std::max(
        kMinTolerance, limits::kSkinToleranceMads * median_absolute_deviation(cb, model.cb_center));
    model.min_brightness = limits::kMinSkinBrightnessRatio * median(y);
    return model;
}

void append_evenly_spaced(std::vector<YCrCb>& samples, const std::vector<YCrCb>& region,
                          std::size_t max_count) {
    const std::size_t count = std::min(region.size(), max_count);
    for (std::size_t k = 0; k < count; ++k) {
        samples.push_back(region[k * region.size() / count]);
    }
}

bool is_skin(const SkinModel& model, const YCrCb& pixel) {
    return pixel.y >= model.min_brightness &&
           std::abs(pixel.cr - model.cr_center) <= model.cr_tolerance &&
           std::abs(pixel.cb - model.cb_center) <= model.cb_tolerance;
}

}  // namespace rppg
