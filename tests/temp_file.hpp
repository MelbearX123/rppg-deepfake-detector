#pragma once

#include <filesystem>
#include <fstream>
#include <string>

// A file in the system temp folder, deleted when the test ends.
class TempFile {
public:
    explicit TempFile(const std::string& name)
        : path_((std::filesystem::temp_directory_path() / ("rppg_test_" + name)).string()) {}
    ~TempFile() { std::filesystem::remove(path_); }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    const std::string& path() const { return path_; }
    void write(const std::string& text) const { std::ofstream(path_) << text; }

private:
    std::string path_;
};
