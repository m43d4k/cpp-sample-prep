#pragma once

#include "core/conversion_settings.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace audio_converter::util {

struct InputPathInspection {
    std::optional<core::InputMode> input_mode;
    std::string normalized_path;
    std::string error_message;
};

std::vector<std::filesystem::path> collect_input_files(const core::ConversionSettings &settings);
InputPathInspection inspect_input_path(const std::filesystem::path &path);
bool has_supported_input_extension(const std::filesystem::path &path);
std::filesystem::path build_output_path(
    const std::filesystem::path &input_path,
    const core::ConversionSettings &settings);
std::filesystem::path output_parent_directory(
    const std::filesystem::path &input_path,
    const core::ConversionSettings &settings);
std::filesystem::path replacement_output_path(
    const std::filesystem::path &input_path,
    core::OutputFormat output_format);
std::string to_display_string(const std::filesystem::path &path);

} // namespace audio_converter::util
