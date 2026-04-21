cmake_minimum_required(VERSION 3.21)

project(cpp_audio_converter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG v1.13.1
    SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

include(CTest)

add_library(audio_converter_lib
    src/core/conversion_settings.cpp
    src/core/log_store.cpp
    src/core/run_conversion.cpp
    src/audio/audio_conversion.cpp
    src/util/native_file_dialog.cpp
    src/util/path_utils.cpp
    src/util/temp_file.cpp
)

target_include_directories(audio_converter_lib
    PUBLIC
        src
        third_party/r8brain-free-src
)
target_link_libraries(audio_converter_lib PUBLIC PkgConfig::SNDFILE)

add_executable(cpp_audio_converter
    src/main.cpp
)

target_include_directories(cpp_audio_converter PRIVATE src)
target_link_libraries(cpp_audio_converter PRIVATE Slint::Slint audio_converter_lib)

slint_target_sources(cpp_audio_converter
    ui/app-window.slint
    NAMESPACE ui
)

if(BUILD_TESTING)
    add_executable(audio_converter_tests
        tests/audio_converter_tests.cpp
    )
    target_link_libraries(audio_converter_tests PRIVATE audio_converter_lib)
    add_test(NAME audio_converter_tests COMMAND audio_converter_tests)
endif()
