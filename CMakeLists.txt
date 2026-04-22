cmake_minimum_required(VERSION 3.21)

project(cpp_audio_converter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AUDIO_CONVERTER_APP_NAME "cpp-audio-converter")
set(AUDIO_CONVERTER_EXECUTABLE_NAME "cpp-audio-converter")
set(AUDIO_CONVERTER_BUNDLE_IDENTIFIER "com.example.cpp-audio-converter" CACHE STRING
    "Bundle identifier used for the macOS app bundle.")
set(AUDIO_CONVERTER_MACOS_MIN_VERSION "11.0" CACHE STRING
    "Minimum macOS version for the app bundle.")

if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${AUDIO_CONVERTER_MACOS_MIN_VERSION}" CACHE STRING
            "Minimum macOS deployment target." FORCE)
    endif()
    enable_language(OBJCXX)
endif()

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

add_library(audio_converter_lib STATIC
    src/audio/audio_file_info.cpp
    src/audio/audio_conversion.cpp
    src/core/conversion_settings.cpp
    src/core/input_preview.cpp
    src/core/log_store.cpp
    src/core/run_conversion.cpp
    src/util/native_file_dialog.cpp
    src/util/path_utils.cpp
    src/util/temp_file.cpp
)

if(APPLE)
    target_sources(audio_converter_lib PRIVATE src/util/native_drop_target_mac.mm)
    target_compile_options(audio_converter_lib PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(audio_converter_lib PRIVATE "-framework AppKit")
else()
    target_sources(audio_converter_lib PRIVATE src/util/native_drop_target.cpp)
endif()

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
set_target_properties(cpp_audio_converter PROPERTIES
    OUTPUT_NAME "${AUDIO_CONVERTER_EXECUTABLE_NAME}"
)

slint_target_sources(cpp_audio_converter
    ui/app-window.slint
    NAMESPACE ui
)

if(APPLE)
    set(AUDIO_CONVERTER_ICON_FILE_NAME "${AUDIO_CONVERTER_APP_NAME}.icns")
    set(AUDIO_CONVERTER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(AUDIO_CONVERTER_GENERATED_ICON "${AUDIO_CONVERTER_GENERATED_DIR}/${AUDIO_CONVERTER_ICON_FILE_NAME}")
    set(AUDIO_CONVERTER_GENERATED_INFO_PLIST "${AUDIO_CONVERTER_GENERATED_DIR}/Info.plist")
    set(AUDIO_CONVERTER_PLACEHOLDER_ICON
        "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/GenericApplicationIcon.icns")

    file(MAKE_DIRECTORY "${AUDIO_CONVERTER_GENERATED_DIR}")

    if(NOT EXISTS "${AUDIO_CONVERTER_PLACEHOLDER_ICON}")
        message(FATAL_ERROR "Placeholder icon was not found at ${AUDIO_CONVERTER_PLACEHOLDER_ICON}")
    endif()

    add_custom_command(
        OUTPUT "${AUDIO_CONVERTER_GENERATED_ICON}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AUDIO_CONVERTER_GENERATED_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${AUDIO_CONVERTER_PLACEHOLDER_ICON}"
            "${AUDIO_CONVERTER_GENERATED_ICON}"
        COMMENT "Copying placeholder macOS app icon"
        VERBATIM
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
        "${AUDIO_CONVERTER_GENERATED_INFO_PLIST}"
        @ONLY
    )

    set_source_files_properties("${AUDIO_CONVERTER_GENERATED_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
        GENERATED TRUE
    )

    target_sources(cpp_audio_converter PRIVATE "${AUDIO_CONVERTER_GENERATED_ICON}")

    set_target_properties(cpp_audio_converter PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "${AUDIO_CONVERTER_APP_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${AUDIO_CONVERTER_BUNDLE_IDENTIFIER}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "${AUDIO_CONVERTER_ICON_FILE_NAME}"
        MACOSX_BUNDLE_INFO_PLIST "${AUDIO_CONVERTER_GENERATED_INFO_PLIST}"
    )

    set(AUDIO_CONVERTER_BUNDLE_SEARCH_DIRS
        ${SNDFILE_LIBRARY_DIRS}
        /opt/homebrew/lib
        /opt/homebrew/opt/libsndfile/lib
        /usr/local/lib
        /usr/local/opt/libsndfile/lib
    )
    list(REMOVE_DUPLICATES AUDIO_CONVERTER_BUNDLE_SEARCH_DIRS)

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/StageMacOSBundle.cmake.in"
        "${AUDIO_CONVERTER_GENERATED_DIR}/StageMacOSBundle.cmake"
        @ONLY
    )

    add_custom_target(stage_macos_app
        COMMAND "${CMAKE_COMMAND}" -DAUDIO_CONVERTER_CREATE_ZIP=OFF
            -P "${AUDIO_CONVERTER_GENERATED_DIR}/StageMacOSBundle.cmake"
        DEPENDS cpp_audio_converter
        COMMENT "Staging a self-contained macOS app bundle"
        VERBATIM
    )

    add_custom_target(package_macos_zip
        COMMAND "${CMAKE_COMMAND}" -DAUDIO_CONVERTER_CREATE_ZIP=ON
            -P "${AUDIO_CONVERTER_GENERATED_DIR}/StageMacOSBundle.cmake"
        DEPENDS cpp_audio_converter
        COMMENT "Packaging a self-contained macOS app bundle as a ZIP archive"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    add_executable(audio_converter_tests
        tests/audio_converter_tests.cpp
    )
    target_compile_definitions(audio_converter_tests PRIVATE AUDIO_CONVERTER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(audio_converter_tests PRIVATE audio_converter_lib)
    add_test(NAME audio_converter_tests COMMAND audio_converter_tests)
endif()

include(CPack)
