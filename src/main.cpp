#include "app-window.h"
#include "core/conversion_settings.hpp"
#include "core/log_store.hpp"
#include "core/run_conversion.hpp"
#include "util/native_file_dialog.hpp"

#include <slint.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace core = audio_converter::core;
namespace util = audio_converter::util;

namespace {

std::shared_ptr<slint::VectorModel<slint::SharedString>> to_log_model(const core::LogStore &log_store)
{
    std::vector<slint::SharedString> rows;
    rows.reserve(log_store.lines().size());
    for (const auto &line : log_store.lines()) {
        rows.emplace_back(line);
    }
    return std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(rows));
}

core::UiSettingsInput read_ui_settings(const ui::MainWindow &window)
{
    return {
        .input_path = std::string(window.get_input_path()),
        .input_mode_index = window.get_input_mode_index(),
        .overwrite_originals = window.get_overwrite_originals(),
        .output_directory = std::string(window.get_output_directory()),
        .file_name_rule_index = window.get_file_name_rule_index(),
        .file_name_affix = std::string(window.get_file_name_affix()),
        .sample_rate_index = window.get_sample_rate_index(),
        .output_format_index = window.get_output_format_index(),
        .bit_depth_index = window.get_bit_depth_index(),
    };
}

void sync_log_view(ui::MainWindow &window, const core::LogStore &log_store)
{
    window.set_log_lines(to_log_model(log_store));
}

void append_log(ui::MainWindow &window, core::LogStore &log_store, std::string line)
{
    log_store.push(std::move(line));
    sync_log_view(window, log_store);
}

slint::SharedString to_shared_string(const std::string &value)
{
    return slint::SharedString(std::string_view(value));
}

template <typename Setter>
void handle_dialog_result(
    ui::MainWindow &window,
    core::LogStore &log_store,
    const util::NativeDialogResult &result,
    Setter &&setter)
{
    if (!result.error_message.empty()) {
        append_log(window, log_store, "Failed: " + result.error_message);
        window.set_status_text("Picker unavailable");
        return;
    }
    if (!result.accepted) {
        return;
    }

    setter(result.path);
}

} // namespace

int main()
{
    auto window = ui::MainWindow::create();
    core::LogStore log_store;
    std::atomic_bool is_running { false };
    std::jthread worker_thread;

    window->set_input_mode_index(0);
    window->set_overwrite_originals(false);
    window->set_file_name_rule_index(0);
    window->set_file_name_affix(to_shared_string(std::string(core::default_file_name_affix(core::FileNameRule::Prefix))));
    window->set_sample_rate_index(0);
    window->set_output_format_index(0);
    window->set_bit_depth_index(1);
    window->set_is_running(false);
    window->set_status_text("Idle");
    window->set_progress_value(0.0f);

    log_store.push("Status: select an input file or folder.");
    log_store.push("Status: supported input formats are WAV / AIFF.");
    sync_log_view(*window, log_store);

    window->on_request_pick_input_file([weak_window = slint::ComponentWeakHandle(window), &log_store] {
        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            return;
        }

        auto window = *maybe_window;
        const auto result = util::pick_input_file();
        handle_dialog_result(*window, log_store, result, [&](const std::string &path) {
            window->set_input_mode_index(0);
            window->set_input_path(to_shared_string(path));
        });
    });

    window->on_request_pick_input_directory([weak_window = slint::ComponentWeakHandle(window), &log_store] {
        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            return;
        }

        auto window = *maybe_window;
        const auto result = util::pick_input_directory();
        handle_dialog_result(*window, log_store, result, [&](const std::string &path) {
            window->set_input_mode_index(1);
            window->set_input_path(to_shared_string(path));
        });
    });

    window->on_request_pick_output_directory([weak_window = slint::ComponentWeakHandle(window), &log_store] {
        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            return;
        }

        auto window = *maybe_window;
        const auto result = util::pick_output_directory();
        handle_dialog_result(*window, log_store, result, [&](const std::string &path) {
            window->set_output_directory(to_shared_string(path));
        });
    });

    window->on_request_run([weak_window = slint::ComponentWeakHandle(window), &log_store, &is_running, &worker_thread] {
        if (is_running.exchange(true)) {
            return;
        }

        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            is_running = false;
            return;
        }

        auto window = *maybe_window;
        log_store.clear();

        const auto build_result = core::build_settings(read_ui_settings(*window));
        if (!build_result.errors.empty()) {
            for (const auto &error : build_result.errors) {
                log_store.push("Failed: " + error);
            }
            window->set_status_text("Validation failed");
            window->set_progress_value(0.0f);
            window->set_is_running(false);
            sync_log_view(*window, log_store);
            is_running = false;
            return;
        }

        window->set_is_running(true);
        window->set_status_text("Running");
        window->set_progress_value(0.0f);
        log_store.push("Status: validation passed.");
        sync_log_view(*window, log_store);

        const auto settings = *build_result.settings;
        worker_thread = std::jthread([weak_window, &log_store, &is_running, settings] {
            const auto result = core::run_conversion(settings, {
                .on_log_line = [weak_window, &log_store](std::string line) {
                    slint::invoke_from_event_loop([weak_window, &log_store, line = std::move(line)]() mutable {
                        const auto maybe_window = weak_window.lock();
                        if (!maybe_window.has_value()) {
                            return;
                        }
                        auto window = *maybe_window;
                        append_log(*window, log_store, std::move(line));
                    });
                },
                .on_progress = [weak_window](float value, std::string status_text) {
                    slint::invoke_from_event_loop([weak_window, value, status_text = std::move(status_text)]() mutable {
                        const auto maybe_window = weak_window.lock();
                        if (!maybe_window.has_value()) {
                            return;
                        }
                        auto window = *maybe_window;
                        window->set_progress_value(value);
                        window->set_status_text(to_shared_string(status_text));
                    });
                },
            });

            slint::invoke_from_event_loop([weak_window, &is_running, result] {
                const auto maybe_window = weak_window.lock();
                if (maybe_window.has_value()) {
                    auto window = *maybe_window;
                    window->set_progress_value(result.progress_value);
                    window->set_status_text(to_shared_string(result.status_text));
                    window->set_is_running(false);
                }
                is_running = false;
            });
        });
    });

    window->run();
    return 0;
}
