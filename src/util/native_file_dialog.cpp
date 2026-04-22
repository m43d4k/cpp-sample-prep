#include "util/native_file_dialog.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sampleprep::util {

namespace {

std::string trim_path(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    if (value.size() > 1 && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::vector<std::string> parse_dialog_output(std::string output)
{
    std::vector<std::string> paths;

    const auto append_part = [&paths](std::string part) {
        part = trim_path(std::move(part));
        if (!part.empty()) {
            paths.push_back(std::move(part));
        }
    };

    if (output.find('\n') != std::string::npos || output.find('\r') != std::string::npos) {
        std::stringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            append_part(std::move(line));
        }
        return paths;
    }

    if (output.find('|') != std::string::npos) {
        std::stringstream stream(output);
        std::string part;
        while (std::getline(stream, part, '|')) {
            append_part(std::move(part));
        }
        return paths;
    }

    append_part(std::move(output));
    return paths;
}

bool executable_exists(const std::string &name)
{
    const char *path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return false;
    }

    std::stringstream stream(path_env);
    std::string segment;
    while (std::getline(stream, segment, ':')) {
        if (segment.empty()) {
            continue;
        }

        const auto candidate = std::filesystem::path(segment) / name;
        if (std::filesystem::exists(candidate)) {
            return true;
        }
    }
    return false;
}

NativeDialogResult run_dialog_command(const std::string &command)
{
    NativeDialogResult result;
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        result.error_message = "failed to launch native dialog";
        return result;
    }

    std::string output;
    std::array<char, 512> buffer {};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    const int exit_code = pclose(pipe);
    if (exit_code != 0) {
        return result;
    }

    result.paths = parse_dialog_output(std::move(output));
    result.accepted = !result.paths.empty();
    return result;
}

#if defined(__APPLE__)
NativeDialogResult run_osascript(const std::vector<std::string> &lines)
{
    std::string command = "/usr/bin/osascript";
    for (const auto &line : lines) {
        command += " -e '";
        command += line;
        command += "'";
    }
    command += " 2>/dev/null";
    return run_dialog_command(command);
}
#endif

} // namespace

NativeDialogResult pick_input_files()
{
#if defined(__APPLE__)
    return run_osascript({
        "set selectedFiles to choose file with prompt \"Select input audio files\" with multiple selections allowed",
        "set outputText to \"\"",
        "repeat with currentFile in selectedFiles",
        "set outputText to outputText & POSIX path of currentFile & linefeed",
        "end repeat",
        "return outputText",
    });
#elif defined(__linux__)
    if (executable_exists("zenity")) {
        return run_dialog_command(
            "zenity --file-selection --multiple --title='Select input audio files' "
            "--file-filter='Audio files | *.wav *.wave *.aif *.aiff *.flac *.mp3 *.ogg *.oga *.caf' 2>/dev/null");
    }
    if (executable_exists("kdialog")) {
        return run_dialog_command(
            "kdialog --getopenfilename . '*.wav *.wave *.aif *.aiff *.flac *.mp3 *.ogg *.oga *.caf' "
            "--multiple --separate-output 2>/dev/null");
    }
    return { .accepted = false, .paths = {}, .error_message = "install zenity or kdialog to use the native file picker on Linux" };
#else
    return { .accepted = false, .paths = {}, .error_message = "native file picker is not implemented for this platform" };
#endif
}

NativeDialogResult pick_input_directory()
{
#if defined(__APPLE__)
    return run_osascript({
        "POSIX path of (choose folder with prompt \"Select input folder\")",
    });
#elif defined(__linux__)
    if (executable_exists("zenity")) {
        return run_dialog_command("zenity --file-selection --directory --title='Select input folder' 2>/dev/null");
    }
    if (executable_exists("kdialog")) {
        return run_dialog_command("kdialog --getexistingdirectory . 2>/dev/null");
    }
    return { .accepted = false, .paths = {}, .error_message = "install zenity or kdialog to use the native folder picker on Linux" };
#else
    return { .accepted = false, .paths = {}, .error_message = "native folder picker is not implemented for this platform" };
#endif
}

NativeDialogResult pick_output_directory()
{
#if defined(__APPLE__)
    return run_osascript({
        "POSIX path of (choose folder with prompt \"Select output folder\")",
    });
#elif defined(__linux__)
    if (executable_exists("zenity")) {
        return run_dialog_command("zenity --file-selection --directory --title='Select output folder' 2>/dev/null");
    }
    if (executable_exists("kdialog")) {
        return run_dialog_command("kdialog --getexistingdirectory . 2>/dev/null");
    }
    return { .accepted = false, .paths = {}, .error_message = "install zenity or kdialog to use the native folder picker on Linux" };
#else
    return { .accepted = false, .paths = {}, .error_message = "native folder picker is not implemented for this platform" };
#endif
}

} // namespace sampleprep::util
