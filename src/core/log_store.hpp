#pragma once

#include <string>
#include <vector>

namespace audio_converter::core {

class LogStore {
public:
    void clear();
    void push(std::string line);

    [[nodiscard]] const std::vector<std::string> &lines() const;

private:
    std::vector<std::string> lines_;
};

} // namespace audio_converter::core
