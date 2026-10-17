cmake_minimum_required(VERSION 3.23)
project(rppg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# --- Signal processing library: no OpenCV, plain vectors/Eigen in and out ---
add_library(rppg_core
    src/signal.cpp
)
target_include_directories(rppg_core PUBLIC include)
target_link_libraries(rppg_core PUBLIC Eigen3::Eigen)
target_compile_options(rppg_core PRIVATE -Wall -Wextra -Wpedantic)

# --- Apps ---
add_executable(rppg_detect apps/rppg_detect.cpp)
target_link_libraries(rppg_detect PRIVATE rppg_core opencv::opencv)
target_compile_options(rppg_detect PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests ---
enable_testing()
find_package(GTest REQUIRED)
add_executable(rppg_tests tests/test_signal.cpp)
target_link_libraries(rppg_tests PRIVATE rppg_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(rppg_tests)
