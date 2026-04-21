#pragma once

#include "core/conversion_settings.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace audio_converter::util {

std::vector<std::filesystem::path> collect_input_files(const core::ConversionSettings &settings);
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
