#pragma once

#include <functional>
#include <string>
#include <vector>

namespace sampleprep::util {

struct NativeDropEvent {
    std::vector<std::string> paths;
    std::string error_message;
};

bool install_native_file_drop_handler(std::function<void(NativeDropEvent)> handler, std::string &error_message);

} // namespace sampleprep::util
