#include "core/input_preview.hpp"

#include "audio/audio_file_info.hpp"
#include "core/conversion_settings.hpp"
#include "util/path_utils.hpp"

#include <array>
#include <filesystem>
#include <optional>

namespace audio_converter::core {

namespace {

OutputMode preview_output_mode(const InputPreviewRequest &request)
{
    if (request.overwrite_originals) {
        return OutputMode::OverwriteOriginals;
    }
    if (request.use_source_file_directory) {
        return OutputMode::WriteNewFilesInSourceDirectory;
    }
    return OutputMode::WriteNewFiles;
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
    const ResolvedInputSelection &input_selection)
{
    const auto output_format = output_format_from_index(request.output_format_index);
    if (!output_format.has_value()) {
        return "-";
    }

    const auto output_mode = preview_output_mode(request);
    if (output_mode == OutputMode::OverwriteOriginals) {
        return util::replacement_output_path(input_path, *output_format).filename().string();
    }

    const auto file_name_rule = file_name_rule_from_index(request.file_name_rule_index);
    if (!file_name_rule.has_value()) {
        return "-";
    }

    const ConversionSettings settings {
        .input_path = input_selection.input_path,
        .input_mode = input_selection.input_mode,
        .selected_input_paths = input_selection.selected_input_paths,
        .output_mode = output_mode,
        .output_directory = request.output_directory,
        .file_name_rule = *file_name_rule,
        .file_name_affix = resolve_file_name_affix(*file_name_rule, request.file_name_affix),
        .output_format = *output_format,
    };
    return util::build_output_path(input_path, settings).filename().string();
}

std::string resolve_output_path(
    const std::filesystem::path &input_path,
    const InputPreviewRequest &request,
    const ResolvedInputSelection &input_selection)
{
    const auto output_format = output_format_from_index(request.output_format_index);
    if (!output_format.has_value()) {
        return "-";
    }

    const auto output_mode = preview_output_mode(request);
    if (output_mode == OutputMode::OverwriteOriginals) {
        return display_directory(util::replacement_output_path(input_path, *output_format).parent_path());
    }

    if (output_mode == OutputMode::WriteNewFiles && request.output_directory.empty()) {
        return "-";
    }

    const auto file_name_rule = file_name_rule_from_index(request.file_name_rule_index);
    if (!file_name_rule.has_value()) {
        return "-";
    }

    const ConversionSettings settings {
        .input_path = input_selection.input_path,
        .input_mode = input_selection.input_mode,
        .selected_input_paths = input_selection.selected_input_paths,
        .output_mode = output_mode,
        .output_directory = request.output_directory,
        .file_name_rule = *file_name_rule,
        .file_name_affix = resolve_file_name_affix(*file_name_rule, request.file_name_affix),
        .output_format = *output_format,
    };
    return output_directory_label(util::output_parent_directory(input_path, settings));
}

} // namespace

InputPreviewResult preview_input_files(const InputPreviewRequest &request)
{
    InputPreviewResult result;
    const auto input_selection = resolve_input_selection(
        request.input_path,
        request.selected_input_paths,
        true);
    if (!input_selection.errors.empty()) {
        result.errors = input_selection.errors;
        return result;
    }
    if (!input_selection.selection.has_value()) {
        return result;
    }

    ConversionSettings settings {
        .input_path = input_selection.selection->input_path,
        .input_mode = input_selection.selection->input_mode,
        .selected_input_paths = input_selection.selection->selected_input_paths,
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
                .output_name = resolve_output_name(file, request, *input_selection.selection),
                .output_path = resolve_output_path(file, request, *input_selection.selection),
                .selected = true,
            });
        }
    } catch (const std::filesystem::filesystem_error &) {
        result.errors.emplace_back("Failed to list input files.");
    }

    return result;
}

} // namespace audio_converter::core
