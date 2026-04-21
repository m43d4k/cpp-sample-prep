#include "util/native_file_dialog.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

namespace audio_converter::util {

namespace {

std::string trim(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    if (value.size() > 1 && value.back() == '/') {
        value.pop_back();
    }
    return value;
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

    result.path = trim(std::move(output));
    result.accepted = !result.path.empty();
    return result;
}

#if defined(__APPLE__)
NativeDialogResult pick_path_with_osascript(const std::string &script)
{
    return run_dialog_command("/usr/bin/osascript -e '" + script + "' 2>/dev/null");
}
#endif

} // namespace

NativeDialogResult pick_input_file()
{
#if defined(__APPLE__)
    return pick_path_with_osascript("POSIX path of (choose file with prompt \"Select input audio file\")");
#elif defined(__linux__)
    if (executable_exists("zenity")) {
        return run_dialog_command(
            "zenity --file-selection --title='Select input audio file' "
            "--file-filter='Audio files | *.wav *.wave *.aif *.aiff *.flac *.mp3 *.ogg *.oga *.caf' 2>/dev/null");
    }
    if (executable_exists("kdialog")) {
        return run_dialog_command(
            "kdialog --getopenfilename . '*.wav *.wave *.aif *.aiff *.flac *.mp3 *.ogg *.oga *.caf' 2>/dev/null");
    }
    return { .accepted = false, .path = {}, .error_message = "install zenity or kdialog to use the native file picker on Linux" };
#else
    return { .accepted = false, .path = {}, .error_message = "native file picker is not implemented for this platform" };
#endif
}

NativeDialogResult pick_input_directory()
{
#if defined(__APPLE__)
    return pick_path_with_osascript("POSIX path of (choose folder with prompt \"Select input folder\")");
#elif defined(__linux__)
    if (executable_exists("zenity")) {
        return run_dialog_command("zenity --file-selection --directory --title='Select input folder' 2>/dev/null");
    }
    if (executable_exists("kdialog")) {
        return run_dialog_command("kdialog --getexistingdirectory . 2>/dev/null");
    }
    return { .accepted = false, .path = {}, .error_message = "install zenity or kdialog to use the native folder picker on Linux" };
#else
    return { .accepted = false, .path = {}, .error_message = "native folder picker is not implemented for this platform" };
#endif
}

NativeDialogResult pick_output_directory()
{
#if defined(__APPLE__)
    return pick_path_with_osascript("POSIX path of (choose folder with prompt \"Select output folder\")");
#elif defined(__linux__)
    if (executable_exists("zenity")) {
        return run_dialog_command("zenity --file-selection --directory --title='Select output folder' 2>/dev/null");
    }
    if (executable_exists("kdialog")) {
        return run_dialog_command("kdialog --getexistingdirectory . 2>/dev/null");
    }
    return { .accepted = false, .path = {}, .error_message = "install zenity or kdialog to use the native folder picker on Linux" };
#else
    return { .accepted = false, .path = {}, .error_message = "native folder picker is not implemented for this platform" };
#endif
}

} // namespace audio_converter::util
