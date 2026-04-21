#pragma once

#include "core/conversion_settings.hpp"

#include <filesystem>
#include <string>

namespace audio_converter::audio {

enum class ProcessStatus {
    Success,
    Skipped,
    Failed,
};

struct ProcessFileRequest {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    core::OutputFormat output_format { core::OutputFormat::Wav };
    int sample_rate { 44100 };
    core::BitDepth bit_depth { core::BitDepth::Pcm16 };
};

struct ProcessFileResult {
    ProcessStatus status { ProcessStatus::Skipped };
    std::string detail;
};

ProcessFileResult convert_audio_file(const ProcessFileRequest &request);

} // namespace audio_converter::audio
