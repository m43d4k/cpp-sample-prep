#include "core/conversion_settings.hpp"

#include <array>
#include <filesystem>
#include <utility>

namespace audio_converter::core {

namespace {

constexpr std::array<int, 4> kSampleRates { 44100, 48000, 88200, 96000 };
constexpr std::array<OutputFormat, 2> kOutputFormats { OutputFormat::Wav, OutputFormat::Aiff };
constexpr std::array<BitDepth, 4> kBitDepths {
    BitDepth::Pcm8,
    BitDepth::Pcm16,
    BitDepth::Pcm24,
    BitDepth::Pcm32,
};
constexpr std::array<FileNameRule, 2> kFileNameRules {
    FileNameRule::Prefix,
    FileNameRule::Postfix,
};

bool contains_path_separator(const std::string &value)
{
    return value.find('/') != std::string::npos;
}

template <typename T, std::size_t N>
std::optional<T> value_from_index(const std::array<T, N> &values, int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= values.size()) {
        return std::nullopt;
    }
    return values[static_cast<std::size_t>(index)];
}

template <std::size_t N>
std::optional<int> value_from_index(const std::array<int, N> &values, int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= values.size()) {
        return std::nullopt;
    }
    return values[static_cast<std::size_t>(index)];
}

} // namespace

BuildSettingsResult build_settings(const UiSettingsInput &input)
{
    BuildSettingsResult result;
    ConversionSettings settings;

    settings.input_path = input.input_path;
    settings.selected_input_paths = input.selected_input_paths;
    settings.output_mode = input.overwrite_originals ? OutputMode::OverwriteOriginals : OutputMode::WriteNewFiles;
    settings.output_directory = input.output_directory;
    settings.file_name_affix = input.file_name_affix;

    if (!settings.selected_input_paths.empty()) {
        settings.input_mode = InputMode::File;
        settings.input_path = settings.selected_input_paths.front().lexically_normal().string();

        for (const auto &selected_path : settings.selected_input_paths) {
            if (!std::filesystem::exists(selected_path)) {
                result.errors.emplace_back("Selected input path does not exist.");
                break;
            }
            if (!std::filesystem::is_regular_file(selected_path)) {
                result.errors.emplace_back("Selected input path must be a file.");
                break;
            }
        }
    } else {
        const std::filesystem::path input_path(input.input_path);

        if (settings.input_path.empty()) {
            result.errors.emplace_back("Input path is required.");
        } else if (!std::filesystem::exists(input_path)) {
            result.errors.emplace_back("Input path does not exist.");
        } else if (std::filesystem::is_regular_file(input_path)) {
            settings.input_mode = InputMode::File;
        } else if (std::filesystem::is_directory(input_path)) {
            settings.input_mode = InputMode::Directory;
        } else {
            result.errors.emplace_back("Input path must be a file or directory.");
        }
    }

    const auto file_name_rule = value_from_index(kFileNameRules, input.file_name_rule_index);
    if (!file_name_rule.has_value()) {
        result.errors.emplace_back("File name rule selection is invalid.");
    } else {
        settings.file_name_rule = *file_name_rule;
    }

    const auto sample_rate = value_from_index(kSampleRates, input.sample_rate_index);
    if (!sample_rate.has_value()) {
        result.errors.emplace_back("Sample rate selection is invalid.");
    } else {
        settings.sample_rate = *sample_rate;
    }

    const auto output_format = value_from_index(kOutputFormats, input.output_format_index);
    if (!output_format.has_value()) {
        result.errors.emplace_back("Output format selection is invalid.");
    } else {
        settings.output_format = *output_format;
    }

    const auto bit_depth = value_from_index(kBitDepths, input.bit_depth_index);
    if (!bit_depth.has_value()) {
        result.errors.emplace_back("Bit depth selection is invalid.");
    } else {
        settings.bit_depth = *bit_depth;
    }

    if (settings.output_mode == OutputMode::WriteNewFiles) {
        if (settings.output_directory.empty()) {
            result.errors.emplace_back("Output directory is required when overwrite is disabled.");
        } else {
            const std::filesystem::path output_directory(settings.output_directory);
            if (!std::filesystem::exists(output_directory)) {
                result.errors.emplace_back("Output directory does not exist.");
            } else if (!std::filesystem::is_directory(output_directory)) {
                result.errors.emplace_back("Output directory must be a directory.");
            }
        }
        if (settings.file_name_affix.empty()) {
            result.errors.emplace_back("Prefix or postfix text is required when overwrite is disabled.");
        } else if (contains_path_separator(settings.file_name_affix)) {
            result.errors.emplace_back("Prefix or postfix text must not contain path separators.");
        }
    }

    if (result.errors.empty()) {
        result.settings = std::move(settings);
    }

    return result;
}

std::string to_string(InputMode value)
{
    switch (value) {
    case InputMode::File:
        return "File";
    case InputMode::Directory:
        return "Directory";
    }
    return "Unknown";
}

std::string to_string(OutputMode value)
{
    switch (value) {
    case OutputMode::OverwriteOriginals:
        return "Overwrite original files";
    case OutputMode::WriteNewFiles:
        return "Write new files";
    }
    return "Unknown";
}

std::string to_string(FileNameRule value)
{
    switch (value) {
    case FileNameRule::Prefix:
        return "Prefix";
    case FileNameRule::Postfix:
        return "Postfix";
    }
    return "Unknown";
}

std::string to_string(OutputFormat value)
{
    switch (value) {
    case OutputFormat::Wav:
        return "wav";
    case OutputFormat::Aiff:
        return "aiff";
    }
    return "unknown";
}

std::string to_string(BitDepth value)
{
    switch (value) {
    case BitDepth::Pcm8:
        return "8 bit PCM";
    case BitDepth::Pcm16:
        return "16 bit PCM";
    case BitDepth::Pcm24:
        return "24 bit PCM";
    case BitDepth::Pcm32:
        return "32 bit PCM";
    }
    return "Unknown";
}

} // namespace audio_converter::core
