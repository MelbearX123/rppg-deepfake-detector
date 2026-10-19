#include "rppg/region_signal.hpp"

#include "rppg/limits.hpp"
#include "rppg/signal.hpp"

namespace rppg {

RegionSample measure_region(const RegionPixels& pixels, const SkinModel& model) {
    std::vector<Color> skin;
    skin.reserve(pixels.colors.size());
    for (std::size_t i = 0; i < pixels.colors.size(); ++i) {
        if (is_skin(model, pixels.ycrcb[i])) {
            skin.push_back(pixels.colors[i]);
        }
    }

    RegionSample sample;
    sample.skin_pixels = skin.size();
    if (!pixels.colors.empty()) {
        sample.coverage = static_cast<double>(skin.size()) / pixels.colors.size();
    }
    sample.color = trimmed_mean(std::move(skin), limits::kTrimFraction);
    return sample;
}

double median_coverage(const RegionSeries& series, const Segment& segment) {
    std::vector<double> coverage;
    coverage.reserve(segment.size());
    for (std::size_t i = segment.begin; i < segment.end; ++i) {
        coverage.push_back(series[i].coverage);
    }
    return coverage.empty() ? 0.0 : median(coverage);
}

bool is_usable(const RegionSeries& series, const Segment& segment) {
    return median_coverage(series, segment) >= limits::kMinRegionCoverage;
}

}  // namespace rppg
