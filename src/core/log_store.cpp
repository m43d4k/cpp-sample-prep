#include "core/log_store.hpp"

#include <utility>

namespace sampleprep::core {

void LogStore::clear()
{
    lines_.clear();
}

void LogStore::push(std::string line)
{
    lines_.push_back(std::move(line));
}

const std::vector<std::string> &LogStore::lines() const
{
    return lines_;
}

} // namespace sampleprep::core
