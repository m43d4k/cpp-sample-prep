#include "util/path_utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace audio_converter::util {

namespace {

constexpr std::array<const char *, 4> kSupportedExtensions {
    ".wav",
    ".wave",
    ".aif",
    ".aiff",
};

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string extension_for(core::OutputFormat output_format)
{
    switch (output_format) {
    case core::OutputFormat::Wav:
        return ".wav";
    case core::OutputFormat::Aiff:
        return ".aiff";
    }
    return ".wav";
}

std::filesystem::path relative_output_directory(
    const std::filesystem::path &input_path,
    const core::ConversionSettings &settings)
{
    if (settings.input_mode != core::InputMode::Directory) {
        return {};
    }

    const auto root_path = std::filesystem::path(settings.input_path).lexically_normal();
    const auto relative_file_path = input_path.lexically_normal().lexically_relative(root_path);
    if (relative_file_path.empty()) {
        return {};
    }

    return relative_file_path.parent_path();
}

} // namespace

std::vector<std::filesystem::path> collect_input_files(const core::ConversionSettings &settings)
{
    std::vector<std::filesystem::path> files;

    if (!settings.selected_input_paths.empty()) {
        files = settings.selected_input_paths;
        std::sort(files.begin(), files.end());
        return files;
    }

    const std::filesystem::path input_path(settings.input_path);

    if (settings.input_mode == core::InputMode::File) {
        files.push_back(input_path);
        return files;
    }

    for (const auto &entry : std::filesystem::recursive_directory_iterator(
             input_path,
             std::filesystem::directory_options::skip_permission_denied)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

bool has_supported_input_extension(const std::filesystem::path &path)
{
    const auto extension = lowercase(path.extension().string());
    return std::find(kSupportedExtensions.begin(), kSupportedExtensions.end(), extension) != kSupportedExtensions.end();
}

std::filesystem::path build_output_path(
    const std::filesystem::path &input_path,
    const core::ConversionSettings &settings)
{
    const auto stem = input_path.stem().string();
    const auto file_name = settings.file_name_rule == core::FileNameRule::Prefix
        ? settings.file_name_affix + stem
        : stem + settings.file_name_affix;
    return output_parent_directory(input_path, settings) / (file_name + extension_for(settings.output_format));
}

std::filesystem::path output_parent_directory(
    const std::filesystem::path &input_path,
    const core::ConversionSettings &settings)
{
    return std::filesystem::path(settings.output_directory) / relative_output_directory(input_path, settings);
}

std::filesystem::path replacement_output_path(
    const std::filesystem::path &input_path,
    core::OutputFormat output_format)
{
    return input_path.parent_path() / (input_path.stem().string() + extension_for(output_format));
}

std::string to_display_string(const std::filesystem::path &path)
{
    return path.lexically_normal().string();
}

} // namespace audio_converter::util
