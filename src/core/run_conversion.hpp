#pragma once

#include "core/conversion_settings.hpp"

#include <functional>
#include <string>

namespace audio_converter::core {

struct RunCallbacks {
    std::function<void(std::string)> on_log_line;
    std::function<void(float, std::string)> on_progress;
};

struct RunConversionResult {
    std::string status_text;
    float progress_value { 0.0f };
    int total_files { 0 };
    int success_count { 0 };
    int failed_count { 0 };
    int skipped_count { 0 };
};

RunConversionResult run_conversion(const ConversionSettings &settings, const RunCallbacks &callbacks = {});

} // namespace audio_converter::core
