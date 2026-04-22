#include "core/run_conversion.hpp"

#include "audio/audio_conversion.hpp"
#include "util/path_utils.hpp"
#include "util/temp_file.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>

namespace audio_converter::core {

namespace {

void emit_log(const RunCallbacks &callbacks, std::string line)
{
    if (callbacks.on_log_line) {
        callbacks.on_log_line(std::move(line));
    }
}

void emit_progress(const RunCallbacks &callbacks, float value, const std::string &status_text)
{
    if (callbacks.on_progress) {
        callbacks.on_progress(value, status_text);
    }
}

void emit_file_complete(const RunCallbacks &callbacks, RunFileUpdate update)
{
    if (callbacks.on_file_complete) {
        callbacks.on_file_complete(std::move(update));
    }
}

std::string make_summary(const RunConversionResult &result)
{
    return "Finished: " + std::to_string(result.success_count) + " success, "
        + std::to_string(result.failed_count) + " failed, "
        + std::to_string(result.skipped_count) + " skipped";
}

std::string display_path(const std::filesystem::path &path)
{
    return util::to_display_string(path);
}

bool ensure_output_parent_directory(const std::filesystem::path &path, std::string &error_message)
{
    if (path.empty()) {
        return true;
    }

    std::error_code error_code;
    std::filesystem::create_directories(path, error_code);
    if (error_code) {
        error_message = "unable to create output directory: " + error_code.message();
        return false;
    }
    return true;
}

} // namespace

RunConversionResult run_conversion(const ConversionSettings &settings, const RunCallbacks &callbacks)
{
    RunConversionResult result;
    const auto inputs = util::collect_input_files(settings);

    if (inputs.empty()) {
        result.status_text = "No eligible files";
        emit_log(callbacks, "Skipped: no supported input files were found.");
        emit_progress(callbacks, 0.0f, result.status_text);
        return result;
    }

    result.total_files = static_cast<int>(inputs.size());
    emit_progress(callbacks, 0.0f, "Running 0/" + std::to_string(result.total_files));

    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const auto &input = inputs[index];
        const auto final_output_path = settings.output_mode == OutputMode::WriteNewFiles
            ? util::build_output_path(input, settings)
            : util::replacement_output_path(input, settings.output_format);
        const auto emit_row_update = [&](RunFileStatus status, std::string detail) {
            emit_file_complete(callbacks, {
                .index = index,
                .input_path = input,
                .output_path = final_output_path,
                .status = status,
                .detail = std::move(detail),
            });
        };

        if (settings.output_mode == OutputMode::WriteNewFiles && std::filesystem::exists(final_output_path)) {
            ++result.skipped_count;
            const std::string detail = "output file already exists: " + display_path(final_output_path);
            emit_log(callbacks, "Skipped: " + display_path(input) + " (" + detail + ")");
            emit_row_update(RunFileStatus::Skipped, detail);
        } else if (
            settings.output_mode == OutputMode::OverwriteOriginals
            && final_output_path != input
            && std::filesystem::exists(final_output_path)) {
            ++result.skipped_count;
            const std::string detail = "replacement path already exists: " + display_path(final_output_path);
            emit_log(callbacks, "Skipped: " + display_path(input) + " (" + detail + ")");
            emit_row_update(RunFileStatus::Skipped, detail);
        } else {
            std::string output_directory_error;
            if (!ensure_output_parent_directory(final_output_path.parent_path(), output_directory_error)) {
                ++result.failed_count;
                emit_log(callbacks, "Failed: " + display_path(input) + " (" + output_directory_error + ")");
                emit_row_update(RunFileStatus::Failed, output_directory_error);
                const auto completed = static_cast<int>(index + 1);
                const auto progress_value = static_cast<float>(completed) / static_cast<float>(result.total_files);
                const auto running_status = completed < result.total_files
                    ? "Running " + std::to_string(completed) + "/" + std::to_string(result.total_files)
                    : make_summary(result);
                result.progress_value = progress_value;
                result.status_text = running_status;
                emit_progress(callbacks, progress_value, running_status);
                continue;
            }

            const auto temp_output_path = util::make_temporary_output_path(final_output_path);
            util::ScopedTempFile scoped_temp_output(temp_output_path);
            const audio::ProcessFileRequest request {
                .input_path = input,
                .output_path = temp_output_path,
                .output_format = settings.output_format,
                .sample_rate = settings.sample_rate,
                .bit_depth = settings.bit_depth,
            };
            const auto file_result = audio::convert_audio_file(request);

            if (file_result.status == audio::ProcessStatus::Success) {
                std::string commit_error;
                bool committed = false;

                if (settings.output_mode == OutputMode::OverwriteOriginals) {
                    committed = util::commit_overwrite(input, temp_output_path, final_output_path, commit_error);
                } else {
                    committed = util::commit_new_file(temp_output_path, final_output_path, commit_error);
                }

                if (committed) {
                    scoped_temp_output.disarm();
                    ++result.success_count;
                    emit_log(callbacks, "Success: " + display_path(input) + " -> " + display_path(final_output_path));
                    emit_row_update(RunFileStatus::Success, {});
                } else {
                    ++result.failed_count;
                    emit_log(callbacks, "Failed: " + display_path(input) + " (" + commit_error + ")");
                    emit_row_update(RunFileStatus::Failed, commit_error);
                }
            } else if (file_result.status == audio::ProcessStatus::Skipped) {
                ++result.skipped_count;
                emit_log(callbacks, "Skipped: " + display_path(input) + " (" + file_result.detail + ")");
                emit_row_update(RunFileStatus::Skipped, file_result.detail);
            } else {
                ++result.failed_count;
                emit_log(callbacks, "Failed: " + display_path(input) + " (" + file_result.detail + ")");
                emit_row_update(RunFileStatus::Failed, file_result.detail);
            }
        }

        const auto completed = static_cast<int>(index + 1);
        const auto progress_value = static_cast<float>(completed) / static_cast<float>(result.total_files);
        const auto running_status = completed < result.total_files
            ? "Running " + std::to_string(completed) + "/" + std::to_string(result.total_files)
            : make_summary(result);
        result.progress_value = progress_value;
        result.status_text = running_status;
        emit_progress(callbacks, progress_value, running_status);
    }

    result.status_text = make_summary(result);
    result.progress_value = 1.0f;
    emit_progress(callbacks, result.progress_value, result.status_text);
    return result;
}

} // namespace audio_converter::core
