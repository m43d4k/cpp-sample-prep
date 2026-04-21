#include "core/input_preview.hpp"

#include "audio/audio_file_info.hpp"
#include "core/conversion_settings.hpp"
#include "util/path_utils.hpp"

#include <array>
#include <filesystem>
#include <optional>

namespace audio_converter::core {

namespace {

constexpr std::array<InputMode, 2> kInputModes {
    InputMode::File,
    InputMode::Directory,
};
constexpr std::array<FileNameRule, 2> kFileNameRules {
    FileNameRule::Prefix,
    FileNameRule::Postfix,
};
constexpr std::array<OutputFormat, 2> kOutputFormats {
    OutputFormat::Wav,
    OutputFormat::Aiff,
};

template <typename T, std::size_t N>
std::optional<T> value_from_index(const std::array<T, N> &values, int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= values.size()) {
        return std::nullopt;
    }
    return values[static_cast<std::size_t>(index)];
}

std::string display_directory(const std::filesystem::path &path)
{
    if (path.empty()) {
        return ".";
    }
    return util::to_display_string(path);
}

std::string output_directory_label(const std::filesystem::path &path)
{
    if (path.empty()) {
        return "-";
    }
    return util::to_display_string(path);
}

std::string format_info_label(const audio::ReadAudioFileInfoResult &result)
{
    if (!result.info.has_value()) {
        return "Unavailable";
    }

    const auto &info = *result.info;
    const auto bit_depth = info.bit_depth.has_value()
        ? to_string(*info.bit_depth)
        : std::string("Unknown bit depth");
    return std::to_string(info.channels) + " ch / "
        + std::to_string(info.sample_rate) + " Hz / "
        + bit_depth;
}

std::string resolve_output_name(
    const std::filesystem::path &input_path,
    const InputPreviewRequest &request,
    InputMode input_mode)
{
    const auto output_format = value_from_index(kOutputFormats, request.output_format_index);
    if (!output_format.has_value()) {
        return "-";
    }

    if (request.overwrite_originals) {
        return util::replacement_output_path(input_path, *output_format).filename().string();
    }

    const auto file_name_rule = value_from_index(kFileNameRules, request.file_name_rule_index);
    if (!file_name_rule.has_value()) {
        return "-";
    }

    const ConversionSettings settings {
        .input_path = request.input_path,
        .input_mode = input_mode,
        .output_mode = OutputMode::WriteNewFiles,
        .output_directory = request.output_directory,
        .file_name_rule = *file_name_rule,
        .file_name_affix = request.file_name_affix.empty()
            ? std::string(default_file_name_affix(*file_name_rule))
            : request.file_name_affix,
        .output_format = *output_format,
    };
    return util::build_output_path(input_path, settings).filename().string();
}

std::string resolve_output_path(
    const std::filesystem::path &input_path,
    const InputPreviewRequest &request,
    InputMode input_mode)
{
    const auto output_format = value_from_index(kOutputFormats, request.output_format_index);
    if (!output_format.has_value()) {
        return "-";
    }

    if (request.overwrite_originals) {
        return display_directory(util::replacement_output_path(input_path, *output_format).parent_path());
    }

    if (request.output_directory.empty()) {
        return "-";
    }

    const auto file_name_rule = value_from_index(kFileNameRules, request.file_name_rule_index);
    if (!file_name_rule.has_value()) {
        return "-";
    }

    const ConversionSettings settings {
        .input_path = request.input_path,
        .input_mode = input_mode,
        .output_mode = OutputMode::WriteNewFiles,
        .output_directory = request.output_directory,
        .file_name_rule = *file_name_rule,
        .file_name_affix = request.file_name_affix.empty()
            ? std::string(default_file_name_affix(*file_name_rule))
            : request.file_name_affix,
        .output_format = *output_format,
    };
    return output_directory_label(util::output_parent_directory(input_path, settings));
}

} // namespace

InputPreviewResult preview_input_files(const InputPreviewRequest &request)
{
    InputPreviewResult result;

    if (request.input_path.empty()) {
        return result;
    }

    if (request.input_mode_index < 0 || static_cast<std::size_t>(request.input_mode_index) >= kInputModes.size()) {
        result.errors.emplace_back("Input mode selection is invalid.");
        return result;
    }

    const auto input_mode = kInputModes[static_cast<std::size_t>(request.input_mode_index)];
    const std::filesystem::path input_path(request.input_path);

    std::error_code error;
    const auto exists = std::filesystem::exists(input_path, error);
    if (error) {
        result.errors.emplace_back("Failed to inspect input path.");
        return result;
    }
    if (!exists) {
        result.errors.emplace_back("Input path does not exist.");
        return result;
    }

    const auto is_regular_file = std::filesystem::is_regular_file(input_path, error);
    if (error) {
        result.errors.emplace_back("Failed to inspect input path.");
        return result;
    }

    const auto is_directory = std::filesystem::is_directory(input_path, error);
    if (error) {
        result.errors.emplace_back("Failed to inspect input path.");
        return result;
    }

    if (input_mode == InputMode::File && !is_regular_file) {
        result.errors.emplace_back("Input path must be a file.");
        return result;
    }
    if (input_mode == InputMode::Directory && !is_directory) {
        result.errors.emplace_back("Input path must be a directory.");
        return result;
    }

    const ConversionSettings settings {
        .input_path = request.input_path,
        .input_mode = input_mode,
    };

    try {
        const auto files = util::collect_input_files(settings);
        result.rows.reserve(files.size());
        for (const auto &file : files) {
            const auto audio_info = audio::read_audio_file_info(file);
            result.rows.push_back({
                .source_path = util::to_display_string(file),
                .input_name = file.filename().string(),
                .input_path = display_directory(file.parent_path()),
                .status = "Pending",
                .info = format_info_label(audio_info),
                .output_name = resolve_output_name(file, request, input_mode),
                .output_path = resolve_output_path(file, request, input_mode),
                .selected = true,
            });
        }
    } catch (const std::filesystem::filesystem_error &) {
        result.errors.emplace_back("Failed to list input files.");
    }

    return result;
}

} // namespace audio_converter::core
