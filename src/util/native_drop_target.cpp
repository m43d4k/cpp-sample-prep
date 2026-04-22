#include "util/native_drop_target.hpp"

namespace sampleprep::util {

bool install_native_file_drop_handler(std::function<void(NativeDropEvent)> /*handler*/, std::string &error_message)
{
    error_message = "drag and drop is not implemented for this platform";
    return false;
}

} // namespace sampleprep::util
