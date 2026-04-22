#include "util/url_launcher.hpp"

#include <array>
#include <spawn.h>
#include <string_view>
#include <sys/wait.h>

extern char **environ;

namespace sampleprep::util {

namespace {

bool is_supported_url_scheme(const std::string &url)
{
    return std::string_view(url).starts_with("https://") || std::string_view(url).starts_with("http://");
}

UrlLaunchResult run_url_opener(const char *command, const std::string &url)
{
    std::array<char *, 3> argv {
        const_cast<char *>(command),
        const_cast<char *>(url.c_str()),
        nullptr,
    };

    pid_t process_id = 0;
    if (posix_spawnp(&process_id, command, nullptr, nullptr, argv.data(), environ) != 0) {
        return { .success = false, .error_message = "failed to start the URL opener" };
    }

    int process_status = 0;
    if (waitpid(process_id, &process_status, 0) == -1) {
        return { .success = false, .error_message = "failed while waiting for the URL opener" };
    }

    if (!WIFEXITED(process_status) || WEXITSTATUS(process_status) != 0) {
        return { .success = false, .error_message = "the URL opener returned an error" };
    }

    return { .success = true, .error_message = {} };
}

} // namespace

UrlLaunchResult open_url(const std::string &url)
{
    if (!is_supported_url_scheme(url)) {
        return { .success = false, .error_message = "unsupported URL scheme" };
    }

#if defined(__APPLE__)
    return run_url_opener("open", url);
#elif defined(__linux__)
    return run_url_opener("xdg-open", url);
#else
    return { .success = false, .error_message = "URL opening is not implemented for this platform" };
#endif
}

} // namespace sampleprep::util
