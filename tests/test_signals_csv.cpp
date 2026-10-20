#include "rppg/output/signals_csv.hpp"

#include "temp_file.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr char kHeader[] = "frame,time_sec,region,r,g,b,skin_pixels,coverage\n";

}  // namespace

TEST(SignalsCsv, ReadsBackWhatWasWritten) {
    // 10 frames; the segment is frames 2-7; frame 4 has no skin in the forehead.
    std::vector<double> timestamps;
    rppg::RegionSignals signals;
    for (std::size_t i = 0; i < 10; ++i) {
        timestamps.push_back(0.05 * static_cast<double>(i));
        for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
            rppg::RegionSample sample;
            sample.color = rppg::Color{100.0 + i, 120.123456 + r, 150.5 + i * r};
            sample.skin_pixels = 300 + i;
            sample.coverage = 0.9;
            signals[r].push_back(sample);
        }
    }
    signals[rppg::kForehead][4].color.reset();
    signals[rppg::kForehead][4].coverage = 0.0;
    const rppg::Segment segment{2, 8};

    const TempFile file("round_trip.csv");
    rppg::write_signals_csv(file.path(), timestamps, signals, segment);
    const rppg::SavedSignals saved = rppg::read_signals_csv(file.path());

    EXPECT_EQ(saved.segment.begin, 2u);
    EXPECT_EQ(saved.segment.end, 8u);
    ASSERT_EQ(saved.timestamps_sec.size(), 8u);
    EXPECT_TRUE(std::isnan(saved.timestamps_sec[1]));  // before the segment: not saved
    for (std::size_t i = segment.begin; i < segment.end; ++i) {
        EXPECT_NEAR(saved.timestamps_sec[i], timestamps[i], 1e-4);
        for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
            const rppg::RegionSample& got = saved.signals[r][i];
            const rppg::RegionSample& want = signals[r][i];
            ASSERT_EQ(got.color.has_value(), want.color.has_value());
            if (want.color) {
                EXPECT_NEAR(got.color->r, want.color->r, 1e-4);
                EXPECT_NEAR(got.color->g, want.color->g, 1e-4);
                EXPECT_NEAR(got.color->b, want.color->b, 1e-4);
            }
            EXPECT_EQ(got.skin_pixels, want.skin_pixels);
            EXPECT_NEAR(got.coverage, want.coverage, 1e-4);
        }
    }
}

TEST(SignalsCsv, MissingFileThrows) {
    EXPECT_THROW(rppg::read_signals_csv("/no/such/signals.csv"), std::runtime_error);
}

TEST(SignalsCsv, WrongHeaderThrows) {
    const TempFile file("wrong_header.csv");
    file.write("frame,time_sec,region\n0,0.0,glabella\n");
    EXPECT_THROW(rppg::read_signals_csv(file.path()), std::runtime_error);
}

TEST(SignalsCsv, MalformedRowThrows) {
    const TempFile file("malformed.csv");
    file.write(std::string(kHeader) + "0,0.0,glabella,1,2,3,10\n");  // a column short
    EXPECT_THROW(rppg::read_signals_csv(file.path()), std::runtime_error);
    file.write(std::string(kHeader) + "0,0.0,chin,1,2,3,10,0.5\n");  // unknown region
    EXPECT_THROW(rppg::read_signals_csv(file.path()), std::runtime_error);
    file.write(std::string(kHeader) + "zero,0.0,glabella,1,2,3,10,0.5\n");  // not a number
    EXPECT_THROW(rppg::read_signals_csv(file.path()), std::runtime_error);
    file.write(std::string(kHeader) + "-1,0.0,glabella,1,2,3,10,0.5\n");  // negative frame
    EXPECT_THROW(rppg::read_signals_csv(file.path()), std::runtime_error);
    file.write(std::string(kHeader) + "0,0.0,glabella,1.5x,2,3,10,0.5\n");  // trailing junk
    EXPECT_THROW(rppg::read_signals_csv(file.path()), std::runtime_error);
}

TEST(SignalsCsv, NoRowsThrows) {
    const TempFile file("empty.csv");
    file.write(kHeader);
    EXPECT_THROW(rppg::read_signals_csv(file.path()), std::runtime_error);
}
