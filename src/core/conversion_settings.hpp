#pragma once

#include <optional>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace audio_converter::core {

enum class InputMode {
    File,
    Directory,
};

enum class OutputMode {
    OverwriteOriginals,
    WriteNewFiles,
    WriteNewFilesInSourceDirectory,
};

enum class FileNameRule {
    Prefix,
    Postfix,
};

enum class OutputFormat {
    Wav,
    Aiff,
};

enum class BitDepth {
    Pcm8,
    Pcm16,
    Pcm24,
    Pcm32,
};

inline constexpr std::string_view kDefaultPrefixAffix = "converted_";
inline constexpr std::string_view kDefaultPostfixAffix = "_converted";

constexpr std::string_view default_file_name_affix(FileNameRule value)
{
    switch (value) {
    case FileNameRule::Prefix:
        return kDefaultPrefixAffix;
    case FileNameRule::Postfix:
        return kDefaultPostfixAffix;
    }
    return kDefaultPrefixAffix;
}

struct UiSettingsInput {
    std::string input_path;
    std::vector<std::filesystem::path> selected_input_paths;
    bool overwrite_originals { false };
    bool use_source_file_directory { false };
    std::string output_directory;
    int file_name_rule_index { 0 };
    std::string file_name_affix { std::string(kDefaultPrefixAffix) };
    int sample_rate_index { 0 };
    int output_format_index { 0 };
    int bit_depth_index { 0 };
};

struct ConversionSettings {
    std::string input_path;
    InputMode input_mode { InputMode::File };
    std::vector<std::filesystem::path> selected_input_paths;
    OutputMode output_mode { OutputMode::WriteNewFiles };
    std::string output_directory;
    FileNameRule file_name_rule { FileNameRule::Prefix };
    std::string file_name_affix { std::string(kDefaultPrefixAffix) };
    int sample_rate { 44100 };
    OutputFormat output_format { OutputFormat::Wav };
    BitDepth bit_depth { BitDepth::Pcm16 };
};

struct BuildSettingsResult {
    std::optional<ConversionSettings> settings;
    std::vector<std::string> errors;
};

struct ResolvedInputSelection {
    std::string input_path;
    InputMode input_mode { InputMode::File };
    std::vector<std::filesystem::path> selected_input_paths;
};

struct ResolveInputSelectionResult {
    std::optional<ResolvedInputSelection> selection;
    std::vector<std::string> errors;
};

std::optional<FileNameRule> file_name_rule_from_index(int index);
std::optional<int> sample_rate_from_index(int index);
std::optional<OutputFormat> output_format_from_index(int index);
std::optional<BitDepth> bit_depth_from_index(int index);
std::string resolve_file_name_affix(
    FileNameRule value,
    std::string_view requested_affix,
    bool use_default_when_empty = true);
ResolveInputSelectionResult resolve_input_selection(
    std::string_view input_path,
    const std::vector<std::filesystem::path> &selected_input_paths,
    bool allow_empty_input = false);
BuildSettingsResult build_settings(const UiSettingsInput &input);

std::string to_string(InputMode value);
std::string to_string(OutputMode value);
std::string to_string(FileNameRule value);
std::string to_string(OutputFormat value);
std::string to_string(BitDepth value);

} // namespace audio_converter::core
