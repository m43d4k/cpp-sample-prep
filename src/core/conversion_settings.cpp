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

std::optional<std::string> validate_selected_input_path(const std::filesystem::path &selected_path)
{
    std::error_code error;
    const auto exists = std::filesystem::exists(selected_path, error);
    if (error) {
        return "Failed to inspect input path.";
    }
    if (!exists) {
        return "Selected input path does not exist.";
    }

    const auto is_regular_file = std::filesystem::is_regular_file(selected_path, error);
    if (error) {
        return "Failed to inspect input path.";
    }
    if (!is_regular_file) {
        return "Selected input path must be a file.";
    }

    return std::nullopt;
}

ResolveInputSelectionResult resolve_single_input_path(std::string_view input_path)
{
    ResolveInputSelectionResult result;
    const std::filesystem::path path(input_path);

    std::error_code error;
    const auto exists = std::filesystem::exists(path, error);
    if (error) {
        result.errors.emplace_back("Failed to inspect input path.");
        return result;
    }
    if (!exists) {
        result.errors.emplace_back("Input path does not exist.");
        return result;
    }

    const auto is_regular_file = std::filesystem::is_regular_file(path, error);
    if (error) {
        result.errors.emplace_back("Failed to inspect input path.");
        return result;
    }

    const auto is_directory = std::filesystem::is_directory(path, error);
    if (error) {
        result.errors.emplace_back("Failed to inspect input path.");
        return result;
    }

    ResolvedInputSelection selection {
        .input_path = std::string(input_path),
    };
    if (is_regular_file) {
        selection.input_mode = InputMode::File;
    } else if (is_directory) {
        selection.input_mode = InputMode::Directory;
    } else {
        result.errors.emplace_back("Input path must be a file or directory.");
        return result;
    }

    result.selection = std::move(selection);
    return result;
}

} // namespace

std::optional<FileNameRule> file_name_rule_from_index(int index)
{
    return value_from_index(kFileNameRules, index);
}

std::optional<int> sample_rate_from_index(int index)
{
    return value_from_index(kSampleRates, index);
}

std::optional<OutputFormat> output_format_from_index(int index)
{
    return value_from_index(kOutputFormats, index);
}

std::optional<BitDepth> bit_depth_from_index(int index)
{
    return value_from_index(kBitDepths, index);
}

std::string resolve_file_name_affix(
    FileNameRule value,
    std::string_view requested_affix,
    bool use_default_when_empty)
{
    if (!requested_affix.empty() || !use_default_when_empty) {
        return std::string(requested_affix);
    }
    return std::string(default_file_name_affix(value));
}

ResolveInputSelectionResult resolve_input_selection(
    std::string_view input_path,
    const std::vector<std::filesystem::path> &selected_input_paths,
    bool allow_empty_input)
{
    ResolveInputSelectionResult result;
    if (!selected_input_paths.empty()) {
        ResolvedInputSelection selection {
            .input_path = selected_input_paths.front().lexically_normal().string(),
            .input_mode = InputMode::File,
            .selected_input_paths = selected_input_paths,
        };

        for (const auto &selected_path : selected_input_paths) {
            if (const auto error = validate_selected_input_path(selected_path); error.has_value()) {
                result.errors.emplace_back(*error);
                return result;
            }
        }

        result.selection = std::move(selection);
        return result;
    }

    if (input_path.empty()) {
        if (!allow_empty_input) {
            result.errors.emplace_back("Input path is required.");
        }
        return result;
    }

    return resolve_single_input_path(input_path);
}

BuildSettingsResult build_settings(const UiSettingsInput &input)
{
    BuildSettingsResult result;
    ConversionSettings settings;

    settings.input_path = input.input_path;
    settings.output_mode = input.overwrite_originals ? OutputMode::OverwriteOriginals : OutputMode::WriteNewFiles;
    settings.output_directory = input.output_directory;
    settings.file_name_affix = input.file_name_affix;

    const auto input_selection = resolve_input_selection(input.input_path, input.selected_input_paths);
    if (!input_selection.errors.empty()) {
        result.errors.insert(result.errors.end(), input_selection.errors.begin(), input_selection.errors.end());
    } else if (input_selection.selection.has_value()) {
        settings.input_path = input_selection.selection->input_path;
        settings.input_mode = input_selection.selection->input_mode;
        settings.selected_input_paths = input_selection.selection->selected_input_paths;
    }

    const auto file_name_rule = file_name_rule_from_index(input.file_name_rule_index);
    if (!file_name_rule.has_value()) {
        result.errors.emplace_back("File name rule selection is invalid.");
    } else {
        settings.file_name_rule = *file_name_rule;
        settings.file_name_affix = resolve_file_name_affix(*file_name_rule, input.file_name_affix, false);
    }

    const auto sample_rate = sample_rate_from_index(input.sample_rate_index);
    if (!sample_rate.has_value()) {
        result.errors.emplace_back("Sample rate selection is invalid.");
    } else {
        settings.sample_rate = *sample_rate;
    }

    const auto output_format = output_format_from_index(input.output_format_index);
    if (!output_format.has_value()) {
        result.errors.emplace_back("Output format selection is invalid.");
    } else {
        settings.output_format = *output_format;
    }

    const auto bit_depth = bit_depth_from_index(input.bit_depth_index);
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
