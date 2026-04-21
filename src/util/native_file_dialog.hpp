#pragma once

#include <string>

namespace audio_converter::util {

struct NativeDialogResult {
    bool accepted { false };
    std::string path;
    std::string error_message;
};

NativeDialogResult pick_input_file();
NativeDialogResult pick_input_directory();
NativeDialogResult pick_output_directory();

} // namespace audio_converter::util
