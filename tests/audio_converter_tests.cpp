#include "audio/audio_conversion.hpp"
#include "core/conversion_settings.hpp"
#include "core/input_preview.hpp"
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
namespace util = audio_converter::util;

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

void write_wave(const fs::path &path, int sample_rate, int format, int frame_count, int channels)
{
    SF_INFO info {};
    info.channels = channels;
    info.samplerate = sample_rate;
    info.format = format;

    SNDFILE *file = sf_open(path.c_str(), SFM_WRITE, &info);
    assert(file != nullptr);

    std::vector<double> frames(static_cast<std::size_t>(frame_count) * static_cast<std::size_t>(channels));
    for (int frame = 0; frame < frame_count; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const double sample = ((frame + channel) % 32) / 32.0;
            frames[static_cast<std::size_t>(frame) * static_cast<std::size_t>(channels)
                + static_cast<std::size_t>(channel)] = channel % 2 == 0 ? sample : -sample;
        }
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
    write_wave(input_file, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 128, 2);

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
    write_wave(input_file, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 128, 2);

    const auto result = audio::convert_audio_file({
        .input_path = input_file,
        .output_path = dir / "tmp.wav",
        .output_format = core::OutputFormat::Wav,
        .sample_rate = 48000,
        .bit_depth = core::BitDepth::Pcm16,
    });
    assert(result.status == audio::ProcessStatus::Skipped);
}

void test_supported_input_extensions()
{
    assert(util::has_supported_input_extension("input.wav"));
    assert(util::has_supported_input_extension("input.aiff"));
    assert(util::has_supported_input_extension("input.flac"));
    assert(util::has_supported_input_extension("input.mp3"));
    assert(util::has_supported_input_extension("input.ogg"));
    assert(util::has_supported_input_extension("input.caf"));
    assert(util::has_supported_input_extension("INPUT.OGA"));
    assert(!util::has_supported_input_extension("input.txt"));
}

void test_directory_conversion()
{
    const auto dir = make_temp_dir();
    const auto input_dir = dir / "input";
    const auto nested_dir = input_dir / "nested";
    const auto output_dir = dir / "output";
    fs::create_directories(nested_dir);
    fs::create_directories(output_dir);

    write_wave(input_dir / "song.wav", 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256, 2);
    write_wave(nested_dir / "deep.wav", 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256, 2);
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

    std::vector<core::RunFileUpdate> updates;
    const auto result = core::run_conversion(settings, {
        .on_file_complete = [&updates](core::RunFileUpdate update) {
            updates.push_back(std::move(update));
        },
    });
    assert(result.total_files == 2);
    assert(result.success_count == 2);
    assert(result.skipped_count == 0);
    assert(updates.size() == 2);
    int callback_success_count = 0;
    int callback_skipped_count = 0;
    for (const auto &update : updates) {
        if (update.status == core::RunFileStatus::Success) {
            ++callback_success_count;
        } else if (update.status == core::RunFileStatus::Skipped) {
            ++callback_skipped_count;
        }
    }
    assert(callback_success_count == 2);
    assert(callback_skipped_count == 0);

    const auto output_path = output_dir / "converted_song.wav";
    assert(fs::exists(output_path));
    const auto output_info = read_info(output_path);
    assert(output_info.samplerate == 44100);
    assert(output_info.channels == 2);

    const auto nested_output_path = output_dir / "nested" / "converted_deep.wav";
    assert(fs::exists(nested_output_path));
    const auto nested_output_info = read_info(nested_output_path);
    assert(nested_output_info.samplerate == 44100);
    assert(nested_output_info.channels == 2);
}

void test_selected_input_paths_filter_unsupported_extensions()
{
    const auto dir = make_temp_dir();
    const auto input_dir = dir / "input";
    const auto output_dir = dir / "output";
    fs::create_directories(input_dir);
    fs::create_directories(output_dir);

    const auto supported_input = input_dir / "song.wav";
    const auto unsupported_input = input_dir / "notes.txt";
    write_wave(supported_input, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256, 2);
    std::ofstream(unsupported_input) << "ignore";

    const core::ConversionSettings settings {
        .input_path = input_dir.string(),
        .input_mode = core::InputMode::Directory,
        .selected_input_paths = { unsupported_input, supported_input },
        .output_mode = core::OutputMode::WriteNewFiles,
        .output_directory = output_dir.string(),
        .file_name_rule = core::FileNameRule::Prefix,
        .file_name_affix = "converted_",
        .sample_rate = 44100,
        .output_format = core::OutputFormat::Wav,
        .bit_depth = core::BitDepth::Pcm16,
    };

    const auto result = core::run_conversion(settings);
    assert(result.total_files == 1);
    assert(result.success_count == 1);
    assert(result.skipped_count == 0);
    assert(fs::exists(output_dir / "converted_song.wav"));
    assert(!fs::exists(output_dir / "converted_notes.wav"));
}

void test_input_preview()
{
    const auto dir = make_temp_dir();
    const auto input_dir = dir / "input";
    const auto nested_dir = input_dir / "nested";
    fs::create_directories(nested_dir);

    write_wave(input_dir / "b-song.wav", 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256, 2);
    write_wave(nested_dir / "c-deep.wav", 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256, 2);
    std::ofstream(input_dir / "a-notes.txt") << "ignore";

    const auto preview = core::preview_input_files({
        .input_path = input_dir.string(),
        .input_mode_index = 1,
        .overwrite_originals = false,
        .output_directory = (dir / "output").string(),
        .file_name_rule_index = 0,
        .file_name_affix = "converted_",
        .output_format_index = 0,
    });
    assert(preview.errors.empty());
    assert(preview.rows.size() == 2);
    assert(preview.rows[0].input_name == "b-song.wav");
    assert(preview.rows[0].status == "Pending");
    assert(preview.rows[0].info.find("2 ch / 48000 Hz / 16 bit PCM") != std::string::npos);
    assert(preview.rows[1].input_name == "c-deep.wav");
    assert(preview.rows[1].input_path.find("nested") != std::string::npos);
    assert(preview.rows[1].output_path.find("output/nested") != std::string::npos);

    const auto single_file_preview = core::preview_input_files({
        .input_path = (input_dir / "b-song.wav").string(),
        .input_mode_index = 0,
        .overwrite_originals = true,
        .output_format_index = 1,
    });
    assert(single_file_preview.errors.empty());
    assert(single_file_preview.rows.size() == 1);
    assert(single_file_preview.rows[0].output_name == "b-song.aiff");

    const auto unsupported_file_preview = core::preview_input_files({
        .input_path = (input_dir / "a-notes.txt").string(),
        .input_mode_index = 0,
        .overwrite_originals = false,
        .output_directory = (dir / "output").string(),
        .file_name_rule_index = 0,
        .file_name_affix = "converted_",
        .output_format_index = 0,
    });
    assert(unsupported_file_preview.errors.empty());
    assert(unsupported_file_preview.rows.empty());
}

void test_overwrite_extension_change()
{
    const auto dir = make_temp_dir();
    const auto input_file = dir / "mix.wav";
    write_wave(input_file, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256, 2);

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

void test_mono_conversion()
{
    const auto dir = make_temp_dir();
    const auto input_file = dir / "mono.wav";
    const auto output_file = dir / "mono_converted.aiff";
    write_wave(input_file, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 256, 1);

    const auto result = audio::convert_audio_file({
        .input_path = input_file,
        .output_path = output_file,
        .output_format = core::OutputFormat::Aiff,
        .sample_rate = 44100,
        .bit_depth = core::BitDepth::Pcm16,
    });
    assert(result.status == audio::ProcessStatus::Success);
    assert(fs::exists(output_file));

    const auto info = read_info(output_file);
    assert((info.format & SF_FORMAT_TYPEMASK) == SF_FORMAT_AIFF);
    assert(info.channels == 1);
    assert(info.samplerate == 44100);
}

} // namespace

int main()
{
    test_build_settings();
    test_same_condition_skip();
    test_supported_input_extensions();
    test_directory_conversion();
    test_selected_input_paths_filter_unsupported_extensions();
    test_input_preview();
    test_overwrite_extension_change();
    test_mono_conversion();
    return 0;
}
