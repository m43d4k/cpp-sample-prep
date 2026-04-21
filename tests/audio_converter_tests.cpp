#include "audio/audio_conversion.hpp"
#include "core/conversion_settings.hpp"
#include "core/run_conversion.hpp"
#include "util/path_utils.hpp"

#include <sndfile.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace core = audio_converter::core;
namespace audio = audio_converter::audio;

namespace {

fs::path make_temp_dir()
{
    const auto base = fs::temp_directory_path() / fs::path("cpp-audio-converter-tests");
    fs::create_directories(base);
    const auto seed = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::mt19937_64 generator(seed);
    std::uniform_int_distribution<unsigned long long> distribution;
    const auto dir = base / ("case-" + std::to_string(distribution(generator)));
    fs::create_directories(dir);
    return dir;
}

void write_stereo_wave(const fs::path &path, int sample_rate, int format, int frame_count)
{
    SF_INFO info {};
    info.channels = 2;
    info.samplerate = sample_rate;
    info.format = format;

    SNDFILE *file = sf_open(path.c_str(), SFM_WRITE, &info);
    assert(file != nullptr);

    std::vector<double> frames(static_cast<std::size_t>(frame_count) * 2);
    for (int frame = 0; frame < frame_count; ++frame) {
        const double sample = (frame % 32) / 32.0;
        frames[static_cast<std::size_t>(frame) * 2] = sample;
        frames[static_cast<std::size_t>(frame) * 2 + 1] = -sample;
    }

    const auto written = sf_writef_double(file, frames.data(), frame_count);
    assert(written == frame_count);
    sf_close(file);
}

SF_INFO read_info(const fs::path &path)
{
    SF_INFO info {};
    SNDFILE *file = sf_open(path.c_str(), SFM_READ, &info);
    assert(file != nullptr);
    sf_close(file);
    return info;
}

void test_build_settings()
{
    const auto dir = make_temp_dir();
    const auto input_file = dir / "input.wav";
    const auto output_dir = dir / "out";
    fs::create_directories(output_dir);
    write_stereo_wave(input_file, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 128);

    const auto result = core::build_settings({
        .input_path = input_file.string(),
        .input_mode_index = 0,
        .overwrite_originals = false,
        .output_directory = output_dir.string(),
        .file_name_rule_index = 0,
        .sample_rate_index = 1,
        .output_format_index = 0,
        .bit_depth_index = 1,
    });
    assert(result.errors.empty());
    assert(result.settings.has_value());
    assert(result.settings->file_name_affix == core::kDefaultPrefixAffix);
    assert(core::default_file_name_affix(core::FileNameRule::Prefix) == core::kDefaultPrefixAffix);
    assert(core::default_file_name_affix(core::FileNameRule::Postfix) == core::kDefaultPostfixAffix);

    const auto invalid = core::build_settings({
        .input_path = input_file.string(),
        .input_mode_index = 0,
        .overwrite_originals = false,
        .output_directory = output_dir.string(),
        .file_name_rule_index = 0,
        .file_name_affix = "bad/name",
        .sample_rate_index = 1,
        .output_format_index = 0,
        .bit_depth_index = 1,
    });
    assert(!invalid.errors.empty());
}

void test_same_condition_skip()
{
    const auto dir = make_temp_dir();
    const auto input_file = dir / "input.wav";
    write_stereo_wave(input_file, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 128);

    const auto result = audio::convert_audio_file({
        .input_path = input_file,
        .output_path = dir / "tmp.wav",
        .output_format = core::OutputFormat::Wav,
        .sample_rate = 48000,
        .bit_depth = core::BitDepth::Pcm16,
    });
    assert(result.status == audio::ProcessStatus::Skipped);
}

void test_directory_conversion()
{
    const auto dir = make_temp_dir();
    const auto input_dir = dir / "input";
    const auto output_dir = dir / "output";
    fs::create_directories(input_dir);
    fs::create_directories(output_dir);

    write_stereo_wave(input_dir / "song.wav", 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256);
    std::ofstream(input_dir / "notes.txt") << "ignore";

    const core::ConversionSettings settings {
        .input_path = input_dir.string(),
        .input_mode = core::InputMode::Directory,
        .output_mode = core::OutputMode::WriteNewFiles,
        .output_directory = output_dir.string(),
        .file_name_rule = core::FileNameRule::Prefix,
        .file_name_affix = "converted_",
        .sample_rate = 44100,
        .output_format = core::OutputFormat::Wav,
        .bit_depth = core::BitDepth::Pcm16,
    };

    const auto result = core::run_conversion(settings);
    assert(result.total_files == 2);
    assert(result.success_count == 1);
    assert(result.skipped_count == 1);

    const auto output_path = output_dir / "converted_song.wav";
    assert(fs::exists(output_path));
    const auto info = read_info(output_path);
    assert(info.samplerate == 44100);
    assert(info.channels == 2);
}

void test_overwrite_extension_change()
{
    const auto dir = make_temp_dir();
    const auto input_file = dir / "mix.wav";
    write_stereo_wave(input_file, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256);

    const core::ConversionSettings settings {
        .input_path = input_file.string(),
        .input_mode = core::InputMode::File,
        .output_mode = core::OutputMode::OverwriteOriginals,
        .output_directory = {},
        .file_name_rule = core::FileNameRule::Prefix,
        .file_name_affix = {},
        .sample_rate = 48000,
        .output_format = core::OutputFormat::Aiff,
        .bit_depth = core::BitDepth::Pcm16,
    };

    const auto result = core::run_conversion(settings);
    assert(result.total_files == 1);
    assert(result.success_count == 1);
    assert(!fs::exists(input_file));

    const auto output_path = dir / "mix.aiff";
    assert(fs::exists(output_path));
    const auto info = read_info(output_path);
    assert((info.format & SF_FORMAT_TYPEMASK) == SF_FORMAT_AIFF);
}

} // namespace

int main()
{
    test_build_settings();
    test_same_condition_skip();
    test_directory_conversion();
    test_overwrite_extension_change();
    return 0;
}
