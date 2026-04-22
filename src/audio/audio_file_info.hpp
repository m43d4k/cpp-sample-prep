#pragma once

#include "core/conversion_settings.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace sampleprep::audio {

struct AudioFileInfo {
    int channels { 0 };
    int sample_rate { 0 };
    std::optional<core::OutputFormat> output_format;
    std::optional<core::BitDepth> bit_depth;
};

struct ReadAudioFileInfoResult {
    std::optional<AudioFileInfo> info;
    std::string error;
};

std::optional<core::OutputFormat> output_format_from_sndfile_format(int format);
std::optional<core::BitDepth> bit_depth_from_sndfile_format(int format);
ReadAudioFileInfoResult read_audio_file_info(const std::filesystem::path &path);

} // namespace sampleprep::audio
