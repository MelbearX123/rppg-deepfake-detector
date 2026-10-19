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
    src/compression.cpp
    src/face_geometry.cpp
    src/face_track.cpp
    src/requirements.cpp
    src/signal.cpp
    src/timing.cpp
    src/track_csv.cpp
)
target_include_directories(rppg_core PUBLIC include)
target_link_libraries(rppg_core PUBLIC Eigen3::Eigen)

# --- Vision library: everything that touches images or video files. ---
add_library(rppg_vision
    src/annotate.cpp
    src/face.cpp
    src/video.cpp
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
    tests/test_requirements.cpp
    tests/test_signal.cpp
    tests/test_timing.cpp
)
target_link_libraries(rppg_tests PRIVATE rppg_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(rppg_tests)
