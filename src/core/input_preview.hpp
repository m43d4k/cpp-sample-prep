#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace audio_converter::core {

struct TargetFileRow {
    std::string source_path;
    std::string input_name;
    std::string input_path;
    std::string status;
    std::string info;
    std::string output_name;
    std::string output_path;
    bool selected { true };
};

struct InputPreviewRequest {
    std::string input_path;
    std::vector<std::filesystem::path> selected_input_paths;
    bool overwrite_originals { false };
    std::string output_directory;
    int file_name_rule_index { 0 };
    std::string file_name_affix;
    int output_format_index { 0 };
};

struct InputPreviewResult {
    std::vector<TargetFileRow> rows;
    std::vector<std::string> errors;
};

InputPreviewResult preview_input_files(const InputPreviewRequest &request);

} // namespace audio_converter::core
