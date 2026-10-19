#include "rppg/pulse/pulse.hpp"

#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kPulseHz = 1.2;  // 72 bpm
constexpr double kPulseDepth = 0.01;
constexpr std::size_t kWindow = 48;

// How much each channel changes with blood volume (Wang et al. 2017): green most.
constexpr rppg::Color kBloodVolume{0.53, 0.77, 0.33};

constexpr rppg::Color kLightSkin{120.0, 150.0, 200.0};
constexpr rppg::Color kDarkSkin{45.0, 60.0, 90.0};

double pulse_wave(std::size_t k, double rate_hz) {
    return std::sin(2.0 * kPi * kPulseHz * static_cast<double>(k) / rate_hz);
}

// Even samples of a skin color whose channels pulse by kPulseDepth along kBloodVolume.
std::vector<rppg::Color> pulsing_skin(const rppg::Color& base, std::size_t count, double rate_hz) {
    std::vector<rppg::Color> colors;
    for (std::size_t k = 0; k < count; ++k) {
        const double wave = kPulseDepth * pulse_wave(k, rate_hz);
        colors.push_back({base.b * (1.0 + kBloodVolume.b * wave),
                          base.g * (1.0 + kBloodVolume.g * wave),
                          base.r * (1.0 + kBloodVolume.r * wave)});
    }
    return colors;
}

double correlation(const std::vector<double>& x, const std::vector<double>& y) {
    const double mx = rppg::mean(x);
    const double my = rppg::mean(y);
    double sxy = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
    }
    return sxy / static_cast<double>(x.size()) / (rppg::stddev(x) * rppg::stddev(y));
}

// Ten frames, 0.1 s apart, with r = i, g = 2i, b = 3i at frame i.
struct Clip {
    rppg::RegionSeries series;
    std::vector<double> timestamps_sec;
};

Clip linear_clip() {
    Clip clip;
    for (std::size_t i = 0; i < 10; ++i) {
        const auto v = static_cast<double>(i);
        clip.timestamps_sec.push_back(0.1 * v);
        rppg::RegionSample sample;
        sample.color = rppg::Color{3.0 * v, 2.0 * v, v};
        clip.series.push_back(sample);
    }
    return clip;
}

}  // namespace

// --- pos_pulse ---

TEST(Pos, FindsThePulse) {
    const double rate = 30.0;
    const auto colors = pulsing_skin(kLightSkin, 300, rate);
    const auto pulse = rppg::pos_pulse(colors, kWindow);

    std::vector<double> wave;
    for (std::size_t k = 0; k < colors.size(); ++k) {
        wave.push_back(pulse_wave(k, rate));
    }
    EXPECT_GT(correlation(pulse, wave), 0.9);
}

TEST(Pos, IgnoresBrightnessChanges) {
    // A slow 20% brightening, same for every channel, with no pulse.
    std::vector<rppg::Color> colors;
    for (std::size_t k = 0; k < 300; ++k) {
        const double scale = 1.0 + 0.2 * static_cast<double>(k) / 300.0;
        colors.push_back({kLightSkin.b * scale, kLightSkin.g * scale, kLightSkin.r * scale});
    }
    for (double v : rppg::pos_pulse(colors, kWindow)) {
        EXPECT_NEAR(v, 0.0, 1e-9);
    }
}

TEST(Pos, IgnoresSkinTone) {
    const auto light = rppg::pos_pulse(pulsing_skin(kLightSkin, 300, 30.0), kWindow);
    const auto dark = rppg::pos_pulse(pulsing_skin(kDarkSkin, 300, 30.0), kWindow);
    for (std::size_t k = 0; k < light.size(); ++k) {
        EXPECT_NEAR(light[k], dark[k], 1e-9);
    }
}

TEST(Pos, FlatColorGivesNoPulse) {
    const std::vector<rppg::Color> colors(100, kLightSkin);
    for (double v : rppg::pos_pulse(colors, kWindow)) {
        EXPECT_NEAR(v, 0.0, 1e-12);
    }
}

TEST(Pos, OutputMatchesInputLength) {
    EXPECT_EQ(rppg::pos_pulse(pulsing_skin(kLightSkin, 100, 30.0), kWindow).size(), 100u);
}

TEST(Pos, BadWindowThrows) {
    const auto colors = pulsing_skin(kLightSkin, 10, 30.0);
    EXPECT_THROW(rppg::pos_pulse(colors, 1), std::invalid_argument);
    EXPECT_THROW(rppg::pos_pulse(colors, 11), std::invalid_argument);
}

TEST(Pos, BlackChannelThrows) {
    // A zero mean can't be normalized; the skin brightness floor should prevent it.
    const std::vector<rppg::Color> colors(10, rppg::Color{0.0, 150.0, 200.0});
    EXPECT_THROW(rppg::pos_pulse(colors, 5), std::invalid_argument);
}

// --- resample_colors ---

TEST(ResampleColors, EvenInputIsUnchanged) {
    const Clip clip = linear_clip();
    const auto colors =
        rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25);
    ASSERT_TRUE(colors);
    ASSERT_EQ(colors->size(), 10u);
    for (std::size_t k = 0; k < 10; ++k) {
        EXPECT_NEAR((*colors)[k].r, static_cast<double>(k), 1e-9);
        EXPECT_NEAR((*colors)[k].b, 3.0 * static_cast<double>(k), 1e-9);
    }
}

TEST(ResampleColors, ShortGapIsBridgedByALine) {
    Clip clip = linear_clip();
    clip.series[4].color.reset();
    const auto colors =
        rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25);
    ASSERT_TRUE(colors);
    EXPECT_NEAR((*colors)[4].r, 4.0, 1e-9);
}

TEST(ResampleColors, LongGapRejectsTheRegion) {
    Clip clip = linear_clip();
    clip.series[3].color.reset();
    clip.series[4].color.reset();  // 0.3 s between frames 2 and 5
    EXPECT_FALSE(rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25));
}

TEST(ResampleColors, MissingFirstFrameHoldsTheNextColor) {
    Clip clip = linear_clip();
    clip.series[0].color.reset();
    const auto colors =
        rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25);
    ASSERT_TRUE(colors);
    ASSERT_EQ(colors->size(), 10u);
    EXPECT_NEAR((*colors)[0].r, 1.0, 1e-9);
}

TEST(ResampleColors, MissingLastFrameHoldsThePreviousColor) {
    Clip clip = linear_clip();
    clip.series[9].color.reset();
    const auto colors =
        rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25);
    ASSERT_TRUE(colors);
    ASSERT_EQ(colors->size(), 10u);
    EXPECT_NEAR((*colors)[9].r, 8.0, 1e-9);
}

TEST(ResampleColors, RepeatedTimestampIsSkipped) {
    // Frame 5 repeats frame 4's time, as a damaged file can.
    Clip clip = linear_clip();
    clip.timestamps_sec[5] = clip.timestamps_sec[4];
    const auto colors =
        rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25);
    ASSERT_TRUE(colors);
    ASSERT_EQ(colors->size(), 10u);
    EXPECT_NEAR((*colors)[4].r, 4.0, 1e-9);
    EXPECT_NEAR((*colors)[5].r, 5.0, 1e-9);  // bridged from 4 (0.4 s) to 6 (0.6 s)
}

TEST(ResampleColors, BackwardTimestampIsSkipped) {
    Clip clip = linear_clip();
    clip.timestamps_sec[5] = 0.25;
    const auto colors =
        rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25);
    ASSERT_TRUE(colors);
    EXPECT_NEAR((*colors)[5].r, 5.0, 1e-9);
}

TEST(ResampleColors, LongGapAtTheEdgeRejectsTheRegion) {
    Clip clip = linear_clip();
    for (std::size_t i = 7; i < 10; ++i) {
        clip.series[i].color.reset();  // last known color is 0.3 s before the end
    }
    EXPECT_FALSE(rppg::resample_colors(clip.series, clip.timestamps_sec, {0, 10}, 10.0, 0.25));
}

TEST(ResampleColors, UsesOnlyTheSegment) {
    const Clip clip = linear_clip();
    const auto colors = rppg::resample_colors(clip.series, clip.timestamps_sec, {2, 8}, 10.0, 0.25);
    ASSERT_TRUE(colors);
    ASSERT_EQ(colors->size(), 6u);
    EXPECT_NEAR(colors->front().r, 2.0, 1e-9);
}

TEST(ResampleColors, UnevenFramesBecomeEvenSamples) {
    // r rises 100 per second, so an even 20 Hz grid should step by 5.
    const std::vector<double> times = {0.0, 0.04, 0.11, 0.15, 0.2};
    rppg::RegionSeries series;
    for (double t : times) {
        rppg::RegionSample sample;
        sample.color = rppg::Color{0.0, 0.0, 100.0 * t};
        series.push_back(sample);
    }
    const auto colors = rppg::resample_colors(series, times, {0, 5}, 20.0, 0.25);
    ASSERT_TRUE(colors);
    ASSERT_EQ(colors->size(), 5u);
    for (std::size_t k = 0; k < 5; ++k) {
        EXPECT_NEAR((*colors)[k].r, 5.0 * static_cast<double>(k), 1e-9);
    }
}

TEST(ResampleColors, NoColorsRejectsTheRegion) {
    const rppg::RegionSeries series(10);
    const Clip clip = linear_clip();
    EXPECT_FALSE(rppg::resample_colors(series, clip.timestamps_sec, {0, 10}, 10.0, 0.25));
}

// --- extract_pulses ---

TEST(ExtractPulses, SkipsUnusableRegionsAndKeepsTheRest) {
    const double fps = 30.0;
    const std::size_t frame_count = 300;
    const auto colors = pulsing_skin(kLightSkin, frame_count, fps);

    std::vector<double> timestamps_sec;
    rppg::RegionSignals signals;
    for (std::size_t i = 0; i < frame_count; ++i) {
        timestamps_sec.push_back(static_cast<double>(i) / fps);
        for (rppg::RegionSeries& series : signals) {
            rppg::RegionSample sample;
            sample.color = colors[i];
            sample.coverage = 1.0;
            series.push_back(sample);
        }
    }
    for (rppg::RegionSample& sample : signals[rppg::kForehead]) {
        sample.coverage = 0.1;  // mostly hair
    }
    for (std::size_t i = 100; i < 130; ++i) {
        signals[rppg::kGlabella][i].color.reset();  // 1 s with no skin
    }

    const rppg::RegionPulses result =
        rppg::extract_pulses(signals, timestamps_sec, {0, frame_count});
    EXPECT_DOUBLE_EQ(result.start_sec, 0.0);
    EXPECT_DOUBLE_EQ(result.rate_hz, rppg::limits::kPulseRateHz);
    EXPECT_FALSE(result.pulses[rppg::kForehead]);
    EXPECT_FALSE(result.pulses[rppg::kGlabella]);
    ASSERT_TRUE(result.pulses[rppg::kLeftMalar]);
    EXPECT_EQ(result.pulses[rppg::kLeftMalar]->size(), frame_count);
}

// --- bandpass_pulses ---

TEST(BandpassPulses, RemovesDriftAndKeepsTheGrid) {
    const double rate = rppg::limits::kPulseRateHz;
    rppg::RegionPulses pulses;
    pulses.start_sec = 2.5;
    pulses.rate_hz = rate;
    std::vector<double> drifting;
    for (std::size_t k = 0; k < 300; ++k) {
        drifting.push_back(pulse_wave(k, rate) + 4.0 + 0.02 * static_cast<double>(k));
    }
    pulses.pulses[rppg::kLeftMalar] = drifting;

    const rppg::RegionPulses result = rppg::bandpass_pulses(pulses);
    EXPECT_DOUBLE_EQ(result.start_sec, 2.5);
    EXPECT_DOUBLE_EQ(result.rate_hz, rate);
    EXPECT_FALSE(result.pulses[rppg::kForehead]);
    ASSERT_TRUE(result.pulses[rppg::kLeftMalar]);
    ASSERT_EQ(result.pulses[rppg::kLeftMalar]->size(), drifting.size());

    // The offset and slope (4 -> 10) are gone; what's left is the pulse.
    std::vector<double> wave;
    for (std::size_t k = 0; k < drifting.size(); ++k) {
        wave.push_back(pulse_wave(k, rate));
    }
    EXPECT_NEAR(rppg::mean(*result.pulses[rppg::kLeftMalar]), 0.0, 0.05);
    EXPECT_GT(correlation(*result.pulses[rppg::kLeftMalar], wave), 0.99);
}
