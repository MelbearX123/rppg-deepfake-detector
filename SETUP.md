# Setting Up a Real C++ Project (CMake + Conan)

In coursework you probably ran `g++ main.cpp -o main`. Real projects split that into three tools:

| Tool | Job | Analogy |
|---|---|---|
| **Conan** | Downloads/builds libraries (OpenCV, Eigen, GoogleTest) | `pip` |
| **CMake** | Describes *what* to build: targets, sources, links | `pyproject.toml` + more |
| **Ninja / Make** | Actually runs the compiler, only on changed files | the thing that calls `g++` |

You write `conanfile.txt` and `CMakeLists.txt`. You never write Makefiles by hand.

---

## 1. Install the tools

> **Already done on this machine.** This section records what was run, so you can redo it elsewhere (or in the Dockerfile later).

No passwordless sudo here, so the build tools come from Homebrew. The compiler is the system GCC 13.3.

```bash
brew install cmake ninja conan ffmpeg
```

```bash
CC=/usr/bin/gcc CXX=/usr/bin/g++ conan profile detect
```

Then these lines were appended to `~/.conan2/profiles/default`:

```ini
[conf]
tools.build:compiler_executables={"c": "/usr/bin/gcc", "cpp": "/usr/bin/g++"}
tools.build:jobs=6

[buildenv]
CMAKE_POLICY_VERSION_MINIMUM=3.5
```

Why: pinning the compiler keeps Homebrew's GCC out of the build, 6 jobs keeps the OpenCV build from running out of RAM, and the CMake policy line lets older dependency recipes build under CMake 4.

On a machine with sudo, the apt route works too: `sudo apt install build-essential cmake ninja-build pipx` then `pipx install conan`.

---

## 2. Create the skeleton

```
rppg/
├── CMakeLists.txt
├── conanfile.txt
├── include/rppg/signal.hpp
├── src/signal.cpp
├── apps/rppg_detect.cpp
└── tests/test_signal.cpp
```

**Headers vs. sources:** `include/rppg/*.hpp` declares functions (the public interface). `src/*.cpp` defines them. Apps and tests `#include <rppg/signal.hpp>` and link against the library. They never include `.cpp` files.

---

## 3. `conanfile.txt`: dependencies

The real file is [conanfile.txt](conanfile.txt). The core of it:

```ini
[requires]
opencv/4.12.0
eigen/3.4.0

[test_requires]
gtest/1.17.0

[generators]
CMakeDeps
CMakeToolchain

[layout]
cmake_layout
```

Below that is an `[options]` section that switches off most of OpenCV and FFmpeg. With defaults they pull in about 50 packages, some needing sudo. Trimmed, it's 13. Check versions at [conan.io/center](https://conan.io/center).

---

## 4. `CMakeLists.txt`: what to build

```cmake
cmake_minimum_required(VERSION 3.23)
project(rppg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets your editor understand the project

find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# --- Library: all the real logic lives here ---
add_library(rppg_core
    src/signal.cpp
)
target_include_directories(rppg_core PUBLIC include)
target_link_libraries(rppg_core PUBLIC Eigen3::Eigen)
target_compile_options(rppg_core PRIVATE -Wall -Wextra -Wpedantic)

# --- Executables: thin wrappers around the library ---
add_executable(rppg_detect apps/rppg_detect.cpp)
target_link_libraries(rppg_detect PRIVATE rppg_core opencv::opencv)

# --- Tests ---
enable_testing()
find_package(GTest REQUIRED)
add_executable(rppg_tests tests/test_signal.cpp)
target_link_libraries(rppg_tests PRIVATE rppg_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(rppg_tests)
```

Key ideas:
- A **target** is something you build (library or executable). Everything is attached to targets with `target_*` commands.
- `PUBLIC` means "I need this, and so does anyone who links me." `PRIVATE` means "only I need this."
- Adding a new `.cpp` means adding it to the `add_library(...)` list.

**Target names:** at the end of `conan install`, Conan prints the exact `find_package(...)` and `target_link_libraries(...)` names for each dependency. If `opencv::opencv` doesn't match what it prints, use what it prints.

---

## 5. Build and run

```bash
conan install . --build=missing
```

The first time, this **compiles OpenCV and FFmpeg from source** and can take 20–60 minutes. That's normal. It's cached afterward. This step also generates `CMakeUserPresets.json`.

```bash
cmake --preset conan-release
```

```bash
cmake --build --preset conan-release
```

```bash
ctest --test-dir build/Release --output-on-failure
```

```bash
./build/Release/rppg_detect data/test_h264.mp4
```

Day to day, after editing code, you only rerun the `cmake --build` line. It recompiles only what changed.

**Debug build** (for the debugger and sanitizers):

```bash
conan install . --build=missing -s build_type=Debug
```

```bash
cmake --preset conan-debug && cmake --build --preset conan-debug
```

**Rule:** debug in Debug, benchmark in Release. Never the other way around.

---

## 6. If the Conan OpenCV build fights you

OpenCV is the hardest package in Conan (video codecs, GUI backends, system packages). If it fails or drags on for **more than half a day**, use the system OpenCV and keep Conan for everything else:

```bash
sudo apt install -y libopencv-dev
```

Remove `opencv/...` from `conanfile.txt`, and in CMake link `${OpenCV_LIBS}` instead of `opencv::opencv`. That's a legitimate engineering decision, not a failure. Don't let packaging eat the week.

---

## 7. `.gitignore`

```
build/
CMakeUserPresets.json
data/
*.avi
*.mp4
```

Never commit build output or dataset videos.

---

## 8. Editor (VS Code)

Install the **WSL**, **clangd**, and **CMake Tools** extensions, and open the folder from WSL (`code .`). clangd reads `build/Release/compile_commands.json` (created by `CMAKE_EXPORT_COMPILE_COMMANDS`) for autocomplete and errors. If it can't find the file, symlink it to the project root:

```bash
ln -s build/Release/compile_commands.json .
```

---

## 9. First milestone: prove it all links

`apps/rppg_detect.cpp` (the real one also decodes every frame):

```cpp
#include <opencv2/opencv.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: rppg_detect <video>\n";
        return 1;
    }
    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
        std::cerr << "could not open " << argv[1] << "\n";
        return 1;
    }
    std::cout << "frames: " << cap.get(cv::CAP_PROP_FRAME_COUNT)
              << "  fps: " << cap.get(cv::CAP_PROP_FPS) << "\n";
}
```

If this builds and prints sensible numbers on a video file, your setup works and Phase 0 is done.

**Common first errors:**
- `undefined reference to ...`: you forgot a `target_link_libraries`, or a `.cpp` isn't listed in `add_library`.
- `fatal error: rppg/signal.hpp: No such file`: missing `target_include_directories`, or a wrong path.
- `Could not find a package configuration file provided by "OpenCV"`: you ran `cmake` directly instead of `cmake --preset conan-release`, so it never saw Conan's files.
- Video opens but has 0 frames: OpenCV was built without FFmpeg. Check with `cv::getBuildInformation()`.
