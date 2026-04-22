#pragma once

#include "core/conversion_settings.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>

namespace audio_converter::core {

enum class RunFileStatus {
    Success,
    Skipped,
    Failed,
};

struct RunFileUpdate {
    std::size_t index { 0 };
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    RunFileStatus status { RunFileStatus::Skipped };
    std::string detail;
};

struct RunCallbacks {
    std::function<void(std::string)> on_log_line;
    std::function<void(float, std::string)> on_progress;
    std::function<void(RunFileUpdate)> on_file_complete;
};

struct RunConversionResult {
    std::string status_text;
    float progress_value { 0.0f };
    int resolved_worker_count { 0 };
    int total_files { 0 };
    int success_count { 0 };
    int failed_count { 0 };
    int skipped_count { 0 };
};

RunConversionResult run_conversion(const ConversionSettings &settings, const RunCallbacks &callbacks = {});

} // namespace audio_converter::core
