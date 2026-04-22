#include "core/run_conversion.hpp"

#include "audio/audio_conversion.hpp"
#include "util/path_utils.hpp"
#include "util/temp_file.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace sampleprep::core {

namespace {

struct RunItem {
    std::size_t index { 0 };
    std::filesystem::path input_path;
    std::filesystem::path final_output_path;
};

struct FileOutcome {
    RunFileStatus status { RunFileStatus::Skipped };
    std::string detail;
};

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

unsigned int resolve_worker_count(unsigned int requested_worker_count, std::size_t input_count)
{
    if (input_count == 0) {
        return 0;
    }

    const unsigned int detected = requested_worker_count == 0
        ? std::max(1u, std::thread::hardware_concurrency())
        : requested_worker_count;
    return std::min<unsigned int>(detected, static_cast<unsigned int>(input_count));
}

std::string display_path(const std::filesystem::path &path)
{
    return util::to_display_string(path);
}

void emit_outcome_update(const RunCallbacks &callbacks, const RunItem &item, const FileOutcome &outcome)
{
    emit_file_complete(callbacks, {
        .index = item.index,
        .input_path = item.input_path,
        .output_path = item.final_output_path,
        .status = outcome.status,
        .detail = outcome.detail,
    });
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

RunItem make_run_item(std::size_t index, const std::filesystem::path &input, const ConversionSettings &settings)
{
    return {
        .index = index,
        .input_path = input,
        .final_output_path = settings.output_mode == OutputMode::OverwriteOriginals
            ? util::replacement_output_path(input, settings.output_format)
            : util::build_output_path(input, settings),
    };
}

std::optional<FileOutcome> preflight_run_item(const RunItem &item, const ConversionSettings &settings)
{
    if (settings.output_mode != OutputMode::OverwriteOriginals && std::filesystem::exists(item.final_output_path)) {
        return FileOutcome {
            .status = RunFileStatus::Skipped,
            .detail = "output file already exists: " + display_path(item.final_output_path),
        };
    }

    if (settings.output_mode == OutputMode::OverwriteOriginals
        && item.final_output_path != item.input_path
        && std::filesystem::exists(item.final_output_path)) {
        return FileOutcome {
            .status = RunFileStatus::Skipped,
            .detail = "replacement path already exists: " + display_path(item.final_output_path),
        };
    }

    std::string output_directory_error;
    if (!ensure_output_parent_directory(item.final_output_path.parent_path(), output_directory_error)) {
        return FileOutcome {
            .status = RunFileStatus::Failed,
            .detail = std::move(output_directory_error),
        };
    }

    return std::nullopt;
}

FileOutcome process_run_item(const RunItem &item, const ConversionSettings &settings)
{
    const auto temp_output_path = util::make_temporary_output_path(item.final_output_path);
    util::ScopedTempFile scoped_temp_output(temp_output_path);
    const audio::ProcessFileRequest request {
        .input_path = item.input_path,
        .output_path = temp_output_path,
        .output_format = settings.output_format,
        .sample_rate = settings.sample_rate,
        .bit_depth = settings.bit_depth,
    };
    const auto file_result = audio::convert_audio_file(request);

    if (file_result.status == audio::ProcessStatus::Success) {
        std::string commit_error;
        const bool committed = settings.output_mode == OutputMode::OverwriteOriginals
            ? util::commit_overwrite(item.input_path, temp_output_path, item.final_output_path, commit_error)
            : util::commit_new_file(temp_output_path, item.final_output_path, commit_error);
        if (committed) {
            scoped_temp_output.disarm();
            return {
                .status = RunFileStatus::Success,
                .detail = {},
            };
        }

        return {
            .status = RunFileStatus::Failed,
            .detail = std::move(commit_error),
        };
    }

    if (file_result.status == audio::ProcessStatus::Skipped) {
        return {
            .status = RunFileStatus::Skipped,
            .detail = std::move(file_result.detail),
        };
    }

    return {
        .status = RunFileStatus::Failed,
        .detail = std::move(file_result.detail),
    };
}

void record_file_outcome(
    RunConversionResult &result,
    const RunCallbacks &callbacks,
    const RunItem &item,
    const FileOutcome &outcome)
{
    switch (outcome.status) {
    case RunFileStatus::Success:
        ++result.success_count;
        emit_log(callbacks, "Success: " + display_path(item.input_path) + " -> " + display_path(item.final_output_path));
        break;
    case RunFileStatus::Skipped:
        ++result.skipped_count;
        emit_log(callbacks, "Skipped: " + display_path(item.input_path) + " (" + outcome.detail + ")");
        break;
    case RunFileStatus::Failed:
        ++result.failed_count;
        emit_log(callbacks, "Failed: " + display_path(item.input_path) + " (" + outcome.detail + ")");
        break;
    }

    emit_outcome_update(callbacks, item, outcome);
}

void update_progress(
    RunConversionResult &result,
    const RunCallbacks &callbacks,
    int completed_files)
{
    const auto progress_value = static_cast<float>(completed_files) / static_cast<float>(result.total_files);
    result.progress_value = progress_value;
    result.status_text = completed_files < result.total_files
        ? "Running " + std::to_string(completed_files) + "/" + std::to_string(result.total_files)
        : make_summary(result);
    emit_progress(callbacks, result.progress_value, result.status_text);
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

    const auto worker_count = resolve_worker_count(settings.cpu_worker_count, inputs.size());
    result.resolved_worker_count = static_cast<int>(worker_count);
    std::atomic_size_t next_index { 0 };
    std::mutex result_mutex;
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);

    for (unsigned int worker_index = 0; worker_index < worker_count; ++worker_index) {
        workers.emplace_back([&settings, &callbacks, &inputs, &next_index, &result_mutex, &result] {
            while (true) {
                const auto index = next_index.fetch_add(1);
                if (index >= inputs.size()) {
                    return;
                }

                const auto item = make_run_item(index, inputs[index], settings);
                const auto preflight_outcome = preflight_run_item(item, settings);
                const auto outcome = preflight_outcome.has_value()
                    ? *preflight_outcome
                    : process_run_item(item, settings);

                std::lock_guard<std::mutex> lock(result_mutex);
                record_file_outcome(result, callbacks, item, outcome);
                update_progress(result, callbacks, result.success_count + result.failed_count + result.skipped_count);
            }
        });
    }

    for (auto &worker : workers) {
        worker.join();
    }

    result.status_text = make_summary(result);
    result.progress_value = 1.0f;
    return result;
}

} // namespace sampleprep::core
