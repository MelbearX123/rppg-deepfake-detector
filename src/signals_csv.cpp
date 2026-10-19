#include "rppg/signals_csv.hpp"

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace rppg {

void write_signals_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                       const RegionSignals& signals, const Segment& segment) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path);
    }

    out << "frame,time_sec,region,r,g,b,skin_pixels,coverage\n";
    out << std::fixed << std::setprecision(4);
    for (std::size_t i = segment.begin; i < segment.end; ++i) {
        for (std::size_t r = 0; r < kRegionCount; ++r) {
            const RegionSample& sample = signals[r][i];
            out << i << ',' << timestamps_sec[i] << ',' << region_name(static_cast<Region>(r))
                << ',';
            if (sample.color) {
                out << sample.color->r << ',' << sample.color->g << ',' << sample.color->b;
            } else {
                out << ",,";
            }
            out << ',' << sample.skin_pixels << ',' << sample.coverage << '\n';
        }
    }
}

}  // namespace rppg
