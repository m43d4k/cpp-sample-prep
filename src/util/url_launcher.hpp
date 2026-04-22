#pragma once

#include <string>

namespace sampleprep::util {

struct UrlLaunchResult {
    bool success { false };
    std::string error_message;
};

UrlLaunchResult open_url(const std::string &url);

} // namespace sampleprep::util
