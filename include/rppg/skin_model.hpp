#pragma once

#include <vector>

namespace rppg {

struct YCrCb {
    double y = 0.0;
    double cr = 0.0;
    double cb = 0.0;
};

struct SkinModel {
    double cr_center = 0.0;
    double cr_tolerance = 0.0;
    double cb_center = 0.0;
    double cb_tolerance = 0.0;
    double min_brightness = 0.0;
};

SkinModel fit_skin_model(const std::vector<YCrCb>& samples);

bool is_skin(const SkinModel& model, const YCrCb& pixel);

}  // namespace rppg
