#include "rppg/signal.hpp"

#include <numeric>

namespace rppg {

double mean(const std::vector<double>& x) {
    if (x.empty()) {
        return 0.0;
    }
    return std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
}

std::vector<double> remove_mean(const std::vector<double>& x) {
    const double m = mean(x);
    std::vector<double> out;
    out.reserve(x.size());
    for (double v : x) {
        out.push_back(v - m);
    }
    return out;
}

}  // namespace rppg
