#include "audio/audio_conversion.hpp"

#include "audio/audio_file_info.hpp"
#include "util/path_utils.hpp"

#include <sndfile.h>

#include "CDSPResampler.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace audio_converter::audio {

namespace {

constexpr int kMinSupportedChannels = 1;
constexpr int kMaxSupportedChannels = 2;
constexpr int kResampleChunkFrames = 4096;

struct SndfileCloser {
    void operator()(SNDFILE *file) const
    {
        if (file != nullptr) {
            sf_close(file);
        }
    }
};

using SndfileHandle = std::unique_ptr<SNDFILE, SndfileCloser>;

bool is_supported_channel_count(int channels)
{
    return channels >= kMinSupportedChannels && channels <= kMaxSupportedChannels;
}

int major_format_for(core::OutputFormat output_format)
{
    switch (output_format) {
    case core::OutputFormat::Wav:
        return SF_FORMAT_WAV;
    case core::OutputFormat::Aiff:
        return SF_FORMAT_AIFF;
    }
    return SF_FORMAT_WAV;
}

int subtype_for(core::OutputFormat output_format, core::BitDepth bit_depth)
{
    switch (bit_depth) {
    case core::BitDepth::Pcm8:
        return output_format == core::OutputFormat::Wav ? SF_FORMAT_PCM_U8 : SF_FORMAT_PCM_S8;
    case core::BitDepth::Pcm16:
        return SF_FORMAT_PCM_16;
    case core::BitDepth::Pcm24:
        return SF_FORMAT_PCM_24;
    case core::BitDepth::Pcm32:
        return SF_FORMAT_PCM_32;
    }
    return SF_FORMAT_PCM_16;
}

bool is_same_conditions(const SF_INFO &input_info, const ProcessFileRequest &request)
{
    const auto input_format = output_format_from_sndfile_format(input_info.format);
    const auto input_bit_depth = bit_depth_from_sndfile_format(input_info.format);
    return input_format.has_value()
        && input_bit_depth.has_value()
        && *input_format == request.output_format
        && input_info.samplerate == request.sample_rate
        && *input_bit_depth == request.bit_depth;
}

ProcessFileResult make_failed(std::string detail)
{
    return { .status = ProcessStatus::Failed, .detail = std::move(detail) };
}

ProcessFileResult make_skipped(std::string detail)
{
    return { .status = ProcessStatus::Skipped, .detail = std::move(detail) };
}

SndfileHandle open_input(const std::filesystem::path &path, SF_INFO &info, ProcessFileResult &error_result)
{
    info = {};
    SndfileHandle file(sf_open(path.c_str(), SFM_READ, &info));
    if (!file) {
        error_result = make_skipped("unable to read input file");
    }
    return file;
}

SndfileHandle open_output(const std::filesystem::path &path, const SF_INFO &info, ProcessFileResult &error_result)
{
    auto output_info = info;
    output_info.frames = 0;
    SndfileHandle file(sf_open(path.c_str(), SFM_WRITE, &output_info));
    if (!file) {
        error_result = make_failed("unable to open output file");
        return nullptr;
    }

    const int clipping = SF_TRUE;
    sf_command(file.get(), SFC_SET_CLIPPING, const_cast<int *>(&clipping), sizeof(clipping));
    return file;
}

bool write_interleaved_frames(
    SNDFILE *output_file,
    const std::vector<double *> &channel_pointers,
    int channel_count,
    sf_count_t frame_count,
    std::vector<double> &interleaved_output)
{
    interleaved_output.resize(static_cast<std::size_t>(frame_count) * static_cast<std::size_t>(channel_count));
    for (sf_count_t frame = 0; frame < frame_count; ++frame) {
        for (int channel = 0; channel < channel_count; ++channel) {
            interleaved_output[static_cast<std::size_t>(frame) * static_cast<std::size_t>(channel_count)
                + static_cast<std::size_t>(channel)] = channel_pointers[static_cast<std::size_t>(channel)][frame];
        }
    }

    return sf_writef_double(output_file, interleaved_output.data(), frame_count) == frame_count;
}

bool copy_without_resampling(SNDFILE *input_file, SNDFILE *output_file, int channel_count)
{
    std::vector<double> interleaved_buffer(
        kResampleChunkFrames * static_cast<std::size_t>(channel_count));

    while (true) {
        const auto read_frames = sf_readf_double(input_file, interleaved_buffer.data(), kResampleChunkFrames);
        if (read_frames == 0) {
            return true;
        }
        if (sf_writef_double(output_file, interleaved_buffer.data(), read_frames) != read_frames) {
            return false;
        }
    }
}

bool copy_with_resampling(SNDFILE *input_file, const SF_INFO &input_info, SNDFILE *output_file, int output_sample_rate)
{
    const int channel_count = input_info.channels;
    std::vector<double> interleaved_input(kResampleChunkFrames * static_cast<std::size_t>(channel_count));
    std::vector<std::vector<double>> channel_input(
        static_cast<std::size_t>(channel_count),
        std::vector<double>(kResampleChunkFrames));
    std::vector<std::unique_ptr<r8b::CDSPResampler24>> resamplers;
    resamplers.reserve(static_cast<std::size_t>(channel_count));

    for (int channel = 0; channel < channel_count; ++channel) {
        resamplers.push_back(std::make_unique<r8b::CDSPResampler24>(
            static_cast<double>(input_info.samplerate),
            static_cast<double>(output_sample_rate),
            kResampleChunkFrames));
    }

    const auto max_output_frames = static_cast<sf_count_t>(resamplers.front()->getMaxOutLen(kResampleChunkFrames));
    std::vector<double *> output_channels(static_cast<std::size_t>(channel_count), nullptr);
    std::vector<double> interleaved_output(
        static_cast<std::size_t>(max_output_frames) * static_cast<std::size_t>(channel_count));
    const auto expected_output_frames = static_cast<sf_count_t>(
        (static_cast<long double>(input_info.frames) * static_cast<long double>(output_sample_rate))
        / static_cast<long double>(input_info.samplerate));

    sf_count_t written_frames = 0;
    bool eof_reached = false;

    while (written_frames < expected_output_frames) {
        sf_count_t frames_to_process = 0;

        if (!eof_reached) {
            const auto read_frames = sf_readf_double(input_file, interleaved_input.data(), kResampleChunkFrames);
            if (read_frames == 0) {
                eof_reached = true;
            } else {
                frames_to_process = read_frames;
                for (sf_count_t frame = 0; frame < read_frames; ++frame) {
                    for (int channel = 0; channel < channel_count; ++channel) {
                        channel_input[channel][static_cast<std::size_t>(frame)] =
                            interleaved_input[static_cast<std::size_t>(frame) * static_cast<std::size_t>(channel_count)
                                + static_cast<std::size_t>(channel)];
                    }
                }
            }
        }

        if (eof_reached) {
            frames_to_process = kResampleChunkFrames;
            for (int channel = 0; channel < channel_count; ++channel) {
                std::fill(channel_input[channel].begin(), channel_input[channel].end(), 0.0);
            }
        }

        int output_frames = -1;
        for (int channel = 0; channel < channel_count; ++channel) {
            double *channel_output = nullptr;
            const int channel_output_frames = resamplers[channel]->process(
                channel_input[channel].data(),
                static_cast<int>(frames_to_process),
                channel_output);

            if (output_frames == -1) {
                output_frames = channel_output_frames;
            } else if (output_frames != channel_output_frames) {
                return false;
            }

            output_channels[channel] = channel_output;
        }

        if (output_frames <= 0) {
            continue;
        }

        const auto remaining_frames = expected_output_frames - written_frames;
        const auto frames_to_write = std::min<sf_count_t>(remaining_frames, output_frames);
        if (!write_interleaved_frames(
                output_file,
                output_channels,
                channel_count,
                frames_to_write,
                interleaved_output)) {
            return false;
        }
        written_frames += frames_to_write;
    }

    return true;
}

} // namespace

ProcessFileResult convert_audio_file(const ProcessFileRequest &request)
{
    if (!util::has_supported_input_extension(request.input_path)) {
        return make_skipped("unsupported input format");
    }

    SF_INFO input_info {};
    ProcessFileResult open_error;
    auto input_file = open_input(request.input_path, input_info, open_error);
    if (!input_file) {
        return open_error;
    }

    if (!output_format_from_sndfile_format(input_info.format).has_value()) {
        return make_skipped("unsupported input format");
    }
    if (!is_supported_channel_count(input_info.channels)) {
        return make_skipped("only mono and stereo input are supported");
    }
    if (is_same_conditions(input_info, request)) {
        return make_skipped("same output conditions");
    }

    SF_INFO output_info {};
    output_info.channels = input_info.channels;
    output_info.samplerate = request.sample_rate;
    output_info.format = major_format_for(request.output_format) | subtype_for(request.output_format, request.bit_depth);

    if (sf_format_check(&output_info) == 0) {
        return make_failed("requested output format is not supported by libsndfile");
    }

    ProcessFileResult output_error;
    auto output_file = open_output(request.output_path, output_info, output_error);
    if (!output_file) {
        return output_error;
    }

    const bool ok = input_info.samplerate == request.sample_rate
        ? copy_without_resampling(input_file.get(), output_file.get(), input_info.channels)
        : copy_with_resampling(input_file.get(), input_info, output_file.get(), request.sample_rate);
    if (!ok) {
        return make_failed("audio conversion failed");
    }

    return { .status = ProcessStatus::Success, .detail = {} };
}

} // namespace audio_converter::audio
