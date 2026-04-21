#include "util/temp_file.hpp"

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace audio_converter::util {

namespace {

std::string random_suffix()
{
    std::random_device device;
    std::uniform_int_distribution<unsigned int> distribution(0U, 0xFFFFFFU);
    return std::to_string(distribution(device));
}

bool rename_file(
    const std::filesystem::path &from,
    const std::filesystem::path &to,
    std::string &error_message)
{
    std::error_code error_code;
    std::filesystem::rename(from, to, error_code);
    if (error_code) {
        error_message = error_code.message();
        return false;
    }
    return true;
}

} // namespace

ScopedTempFile::ScopedTempFile(std::filesystem::path path)
    : path_(std::move(path))
{
}

ScopedTempFile::~ScopedTempFile()
{
    if (!armed_) {
        return;
    }

    std::error_code error_code;
    std::filesystem::remove(path_, error_code);
}

void ScopedTempFile::disarm()
{
    armed_ = false;
}

const std::filesystem::path &ScopedTempFile::path() const
{
    return path_;
}

std::filesystem::path make_temporary_output_path(const std::filesystem::path &final_output_path)
{
    return final_output_path.parent_path()
        / ("." + final_output_path.stem().string() + ".tmp." + random_suffix() + final_output_path.extension().string());
}

bool commit_new_file(
    const std::filesystem::path &temp_output_path,
    const std::filesystem::path &final_output_path,
    std::string &error_message)
{
    if (std::filesystem::exists(final_output_path)) {
        error_message = "output file already exists";
        return false;
    }
    return rename_file(temp_output_path, final_output_path, error_message);
}

bool commit_overwrite(
    const std::filesystem::path &input_path,
    const std::filesystem::path &temp_output_path,
    const std::filesystem::path &final_output_path,
    std::string &error_message)
{
    if (final_output_path == input_path) {
        return rename_file(temp_output_path, final_output_path, error_message);
    }

    if (std::filesystem::exists(final_output_path)) {
        error_message = "replacement path already exists";
        return false;
    }
    if (!rename_file(temp_output_path, final_output_path, error_message)) {
        return false;
    }

    std::error_code error_code;
    std::filesystem::remove(input_path, error_code);
    if (error_code) {
        error_message = "converted file was created, but the original file could not be removed: " + error_code.message();
        return false;
    }

    return true;
}

} // namespace audio_converter::util
