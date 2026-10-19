cmake_minimum_required(VERSION 3.23)
project(rppg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Applies to every target defined below (dependencies are prebuilt by Conan).
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# --- Core library: signal math, no OpenCV. Fast to build, easy to test. ---
add_library(rppg_core
    src/face/face_geometry.cpp
    src/face/face_track.cpp
    src/heart_rate/heart_rate.cpp
    src/heart_rate/spectrum.cpp
    src/output/signals_csv.cpp
    src/output/track_csv.cpp
    src/pulse/filter.cpp
    src/pulse/pulse.cpp
    src/pulse/signal.cpp
    src/regions/pixel_stats.cpp
    src/regions/region_signal.cpp
    src/regions/regions.cpp
    src/regions/skin_model.cpp
    src/requirements.cpp
    src/video/compression.cpp
    src/video/timing.cpp
)
target_include_directories(rppg_core PUBLIC include)
target_link_libraries(rppg_core PUBLIC Eigen3::Eigen)

# --- Vision library: everything that touches images or video files. ---
add_library(rppg_vision
    src/face/face.cpp
    src/output/annotate.cpp
    src/regions/region_sampler.cpp
    src/video/video.cpp
)
target_link_libraries(rppg_vision PUBLIC rppg_core opencv::opencv)

# --- Apps ---
add_executable(rppg_detect apps/rppg_detect.cpp)
target_link_libraries(rppg_detect PRIVATE rppg_vision)

# --- Tests ---
enable_testing()
find_package(GTest REQUIRED)
add_executable(rppg_tests
    tests/test_compression.cpp
    tests/test_face_track.cpp
    tests/test_filter.cpp
    tests/test_heart_rate.cpp
    tests/test_pixel_stats.cpp
    tests/test_pulse.cpp
    tests/test_region_signal.cpp
    tests/test_regions.cpp
    tests/test_requirements.cpp
    tests/test_signal.cpp
    tests/test_skin_model.cpp
    tests/test_spectrum.cpp
    tests/test_timing.cpp
)
target_link_libraries(rppg_tests PRIVATE rppg_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(rppg_tests)
