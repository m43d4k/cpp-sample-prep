#pragma once

#include <filesystem>
#include <string>

namespace audio_converter::util {

class ScopedTempFile {
public:
    explicit ScopedTempFile(std::filesystem::path path);
    ~ScopedTempFile();

    ScopedTempFile(const ScopedTempFile &) = delete;
    ScopedTempFile &operator=(const ScopedTempFile &) = delete;

    void disarm();
    [[nodiscard]] const std::filesystem::path &path() const;

private:
    std::filesystem::path path_;
    bool armed_ { true };
};

std::filesystem::path make_temporary_output_path(const std::filesystem::path &final_output_path);
bool commit_new_file(
    const std::filesystem::path &temp_output_path,
    const std::filesystem::path &final_output_path,
    std::string &error_message);
bool commit_overwrite(
    const std::filesystem::path &input_path,
    const std::filesystem::path &temp_output_path,
    const std::filesystem::path &final_output_path,
    std::string &error_message);

} // namespace audio_converter::util
