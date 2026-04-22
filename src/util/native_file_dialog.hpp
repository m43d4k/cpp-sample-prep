#pragma once

#include <string>
#include <vector>

namespace sampleprep::util {

struct NativeDialogResult {
    bool accepted { false };
    std::vector<std::string> paths;
    std::string error_message;
};

NativeDialogResult pick_input_files();
NativeDialogResult pick_input_directory();
NativeDialogResult pick_output_directory();

} // namespace sampleprep::util
