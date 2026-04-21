#include "app-window.h"
#include "core/conversion_settings.hpp"
#include "core/input_preview.hpp"
#include "core/log_store.hpp"
#include "core/run_conversion.hpp"
#include "util/native_file_dialog.hpp"
#include "util/path_utils.hpp"

#include <slint.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace core = audio_converter::core;
namespace util = audio_converter::util;

namespace {

struct TargetFileTableState {
    std::vector<core::TargetFileRow> rows;
    std::shared_ptr<slint::VectorModel<ui::TargetFileRow>> model;
};

std::shared_ptr<slint::VectorModel<slint::SharedString>> to_string_model(const std::vector<std::string> &rows)
{
    std::vector<slint::SharedString> model_rows;
    model_rows.reserve(rows.size());
    for (const auto &row : rows) {
        model_rows.emplace_back(row);
    }
    return std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(model_rows));
}

std::shared_ptr<slint::VectorModel<slint::SharedString>> to_log_model(const core::LogStore &log_store)
{
    return to_string_model(log_store.lines());
}

slint::SharedString to_shared_string(const std::string &value)
{
    return slint::SharedString(std::string_view(value));
}

ui::TargetFileRow to_ui_target_file_row(const core::TargetFileRow &row)
{
    return {
        .input = to_shared_string(row.input_name),
        .input_path = to_shared_string(row.input_path),
        .status = to_shared_string(row.status),
        .info = to_shared_string(row.info),
        .output = to_shared_string(row.output_name),
        .output_path = to_shared_string(row.output_path),
    };
}

std::shared_ptr<slint::VectorModel<ui::TargetFileRow>> to_target_file_model(const std::vector<core::TargetFileRow> &rows)
{
    std::vector<ui::TargetFileRow> model_rows;
    model_rows.reserve(rows.size());
    for (const auto &row : rows) {
        model_rows.push_back(to_ui_target_file_row(row));
    }
    return std::make_shared<slint::VectorModel<ui::TargetFileRow>>(std::move(model_rows));
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

core::InputPreviewRequest read_input_preview_request(const ui::MainWindow &window)
{
    return {
        .input_path = std::string(window.get_input_path()),
        .input_mode_index = window.get_input_mode_index(),
        .overwrite_originals = window.get_overwrite_originals(),
        .output_directory = std::string(window.get_output_directory()),
        .file_name_rule_index = window.get_file_name_rule_index(),
        .file_name_affix = std::string(window.get_file_name_affix()),
        .output_format_index = window.get_output_format_index(),
    };
}

void sync_log_view(ui::MainWindow &window, const core::LogStore &log_store)
{
    window.set_log_lines(to_log_model(log_store));
}

void sync_input_preview(ui::MainWindow &window, TargetFileTableState &table_state)
{
    const auto preview = core::preview_input_files(read_input_preview_request(window));
    table_state.rows = preview.rows;
    table_state.model = to_target_file_model(table_state.rows);
    window.set_target_file_rows(table_state.model);

    if (!preview.errors.empty()) {
        window.set_target_files_summary(to_shared_string(preview.errors.front()));
    } else if (preview.rows.empty()) {
        const auto input_path = std::string(window.get_input_path());
        window.set_target_files_summary(to_shared_string(
            input_path.empty()
                ? "Select an input file or folder to preview targets."
                : "No input files found."));
    } else {
        window.set_target_files_summary(to_shared_string(
            std::to_string(preview.rows.size()) + " file(s) listed from the current input."));
    }
}

void append_log(ui::MainWindow &window, core::LogStore &log_store, std::string line)
{
    log_store.push(std::move(line));
    sync_log_view(window, log_store);
}

std::string target_file_status_text(const core::RunFileUpdate &update)
{
    switch (update.status) {
    case core::RunFileStatus::Success:
        return "Success";
    case core::RunFileStatus::Skipped:
        return update.detail.empty() ? "Skipped" : "Skipped: " + update.detail;
    case core::RunFileStatus::Failed:
        return update.detail.empty() ? "Failed" : "Failed: " + update.detail;
    }
    return "Pending";
}

void update_target_file_status(
    ui::MainWindow &window,
    TargetFileTableState &table_state,
    const core::RunFileUpdate &update)
{
    if (update.index >= table_state.rows.size() || !table_state.model) {
        return;
    }

    auto &row = table_state.rows[update.index];
    row.status = target_file_status_text(update);
    row.output_name = update.output_path.empty() ? row.output_name : update.output_path.filename().string();
    row.output_path = update.output_path.empty()
        ? row.output_path
        : (update.output_path.parent_path().empty()
            ? "."
            : util::to_display_string(update.output_path.parent_path()));
    table_state.model->set_row_data(update.index, to_ui_target_file_row(row));
    window.set_target_file_rows(table_state.model);
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
    TargetFileTableState target_file_table;
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
    window->set_target_files_summary(to_shared_string("Select an input file or folder to preview targets."));
    target_file_table.model = to_target_file_model({});
    window->set_target_file_rows(target_file_table.model);

    log_store.push("Status: select an input file or folder.");
    log_store.push("Status: supported input formats are WAV / AIFF.");
    sync_log_view(*window, log_store);

    window->on_request_pick_input_file([weak_window = slint::ComponentWeakHandle(window), &log_store, &target_file_table] {
        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            return;
        }

        auto window = *maybe_window;
        const auto result = util::pick_input_file();
        handle_dialog_result(*window, log_store, result, [&](const std::string &path) {
            window->set_input_mode_index(0);
            window->set_input_path(to_shared_string(path));
            sync_input_preview(*window, target_file_table);
        });
    });

    window->on_request_pick_input_directory([weak_window = slint::ComponentWeakHandle(window), &log_store, &target_file_table] {
        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            return;
        }

        auto window = *maybe_window;
        const auto result = util::pick_input_directory();
        handle_dialog_result(*window, log_store, result, [&](const std::string &path) {
            window->set_input_mode_index(1);
            window->set_input_path(to_shared_string(path));
            sync_input_preview(*window, target_file_table);
        });
    });

    window->on_request_pick_output_directory([weak_window = slint::ComponentWeakHandle(window), &log_store, &target_file_table] {
        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            return;
        }

        auto window = *maybe_window;
        const auto result = util::pick_output_directory();
        handle_dialog_result(*window, log_store, result, [&](const std::string &path) {
            window->set_output_directory(to_shared_string(path));
            sync_input_preview(*window, target_file_table);
        });
    });

    window->on_request_refresh_input_preview([weak_window = slint::ComponentWeakHandle(window), &target_file_table] {
        const auto maybe_window = weak_window.lock();
        if (!maybe_window.has_value()) {
            return;
        }

        auto window = *maybe_window;
        sync_input_preview(*window, target_file_table);
    });

    window->on_request_run([weak_window = slint::ComponentWeakHandle(window), &log_store, &target_file_table, &is_running, &worker_thread] {
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
        sync_input_preview(*window, target_file_table);
        log_store.push("Status: validation passed.");
        sync_log_view(*window, log_store);

        const auto settings = *build_result.settings;
        worker_thread = std::jthread([weak_window, &log_store, &target_file_table, &is_running, settings] {
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
                .on_file_complete = [weak_window, &target_file_table](core::RunFileUpdate update) {
                    slint::invoke_from_event_loop(
                        [weak_window, &target_file_table, update = std::move(update)]() mutable {
                            const auto maybe_window = weak_window.lock();
                            if (!maybe_window.has_value()) {
                                return;
                            }
                            auto window = *maybe_window;
                            update_target_file_status(*window, target_file_table, update);
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

    sync_input_preview(*window, target_file_table);
    window->run();
    return 0;
}
