#include "audio/audio_file_info.hpp"

#include <sndfile.h>

namespace sampleprep::audio {

std::optional<core::OutputFormat> output_format_from_sndfile_format(int format)
{
    switch (format & SF_FORMAT_TYPEMASK) {
    case SF_FORMAT_WAV:
        return core::OutputFormat::Wav;
    case SF_FORMAT_AIFF:
        return core::OutputFormat::Aiff;
    default:
        return std::nullopt;
    }
}

std::optional<core::BitDepth> bit_depth_from_sndfile_format(int format)
{
    switch (format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_S8:
    case SF_FORMAT_PCM_U8:
        return core::BitDepth::Pcm8;
    case SF_FORMAT_PCM_16:
        return core::BitDepth::Pcm16;
    case SF_FORMAT_PCM_24:
        return core::BitDepth::Pcm24;
    case SF_FORMAT_PCM_32:
        return core::BitDepth::Pcm32;
    default:
        return std::nullopt;
    }
}

ReadAudioFileInfoResult read_audio_file_info(const std::filesystem::path &path)
{
    SF_INFO sndfile_info {};
    SNDFILE *file = sf_open(path.c_str(), SFM_READ, &sndfile_info);
    if (file == nullptr) {
        const char *message = sf_strerror(nullptr);
        return {
            .info = std::nullopt,
            .error = message != nullptr && message[0] != '\0'
                ? std::string(message)
                : "unable to read input file",
        };
    }

    sf_close(file);
    return {
        .info = AudioFileInfo {
            .channels = sndfile_info.channels,
            .sample_rate = sndfile_info.samplerate,
            .output_format = output_format_from_sndfile_format(sndfile_info.format),
            .bit_depth = bit_depth_from_sndfile_format(sndfile_info.format),
        },
        .error = {},
    };
}

} // namespace sampleprep::audio
