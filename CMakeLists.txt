cmake_minimum_required(VERSION 3.21)

project(SamplePrep VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SAMPLEPREP_APP_NAME "SamplePrep")
set(SAMPLEPREP_EXECUTABLE_NAME "SamplePrep")
set(SAMPLEPREP_BUNDLE_IDENTIFIER "com.example.sampleprep" CACHE STRING
    "Bundle identifier used for the macOS app bundle.")
set(SAMPLEPREP_MACOS_MIN_VERSION "11.0" CACHE STRING
    "Minimum macOS version for the app bundle.")

if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${SAMPLEPREP_MACOS_MIN_VERSION}" CACHE STRING
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

add_library(sampleprep_lib STATIC
    src/audio/audio_file_info.cpp
    src/audio/audio_conversion.cpp
    src/core/conversion_settings.cpp
    src/core/input_preview.cpp
    src/core/log_store.cpp
    src/core/run_conversion.cpp
    src/util/native_file_dialog.cpp
    src/util/path_utils.cpp
    src/util/temp_file.cpp
    src/util/url_launcher.cpp
)

if(APPLE)
    target_sources(sampleprep_lib PRIVATE src/util/native_drop_target_mac.mm)
    target_compile_options(sampleprep_lib PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(sampleprep_lib PRIVATE "-framework AppKit")
else()
    target_sources(sampleprep_lib PRIVATE src/util/native_drop_target.cpp)
endif()

target_include_directories(sampleprep_lib
    PUBLIC
        src
        third_party/r8brain-free-src
)
target_link_libraries(sampleprep_lib PUBLIC PkgConfig::SNDFILE)

add_executable(sampleprep
    src/main.cpp
)

target_compile_definitions(sampleprep PRIVATE SAMPLEPREP_APP_VERSION="${PROJECT_VERSION}")
target_include_directories(sampleprep PRIVATE src)
target_link_libraries(sampleprep PRIVATE Slint::Slint sampleprep_lib)
set_target_properties(sampleprep PROPERTIES
    OUTPUT_NAME "${SAMPLEPREP_EXECUTABLE_NAME}"
)

slint_target_sources(sampleprep
    ui/app-window.slint
    NAMESPACE ui
)

if(APPLE)
    find_program(SAMPLEPREP_CODESIGN_EXECUTABLE codesign REQUIRED)

    set(SAMPLEPREP_ICON_FILE_NAME "${SAMPLEPREP_APP_NAME}.icns")
    set(SAMPLEPREP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(SAMPLEPREP_GENERATED_ICON "${SAMPLEPREP_GENERATED_DIR}/${SAMPLEPREP_ICON_FILE_NAME}")
    set(SAMPLEPREP_GENERATED_INFO_PLIST "${SAMPLEPREP_GENERATED_DIR}/Info.plist")
    set(SAMPLEPREP_TERMINAL_LAUNCHER "${CMAKE_CURRENT_BINARY_DIR}/cpp_audio_converter")
    set(SAMPLEPREP_SOURCE_ICON
        "${CMAKE_SOURCE_DIR}/assets/macos/${SAMPLEPREP_ICON_FILE_NAME}")

    file(MAKE_DIRECTORY "${SAMPLEPREP_GENERATED_DIR}")

    if(NOT EXISTS "${SAMPLEPREP_SOURCE_ICON}")
        message(FATAL_ERROR "App icon was not found at ${SAMPLEPREP_SOURCE_ICON}")
    endif()

    add_custom_command(
        OUTPUT "${SAMPLEPREP_GENERATED_ICON}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SAMPLEPREP_GENERATED_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SAMPLEPREP_SOURCE_ICON}"
            "${SAMPLEPREP_GENERATED_ICON}"
        COMMENT "Copying macOS app icon"
        VERBATIM
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
        "${SAMPLEPREP_GENERATED_INFO_PLIST}"
        @ONLY
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/LaunchSamplePrep.in"
        "${SAMPLEPREP_TERMINAL_LAUNCHER}"
        @ONLY
    )
    file(CHMOD "${SAMPLEPREP_TERMINAL_LAUNCHER}"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )

    set_source_files_properties("${SAMPLEPREP_GENERATED_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
        GENERATED TRUE
    )

    target_sources(sampleprep PRIVATE "${SAMPLEPREP_GENERATED_ICON}")

    set_target_properties(sampleprep PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "${SAMPLEPREP_APP_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${SAMPLEPREP_BUNDLE_IDENTIFIER}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "${SAMPLEPREP_ICON_FILE_NAME}"
        MACOSX_BUNDLE_INFO_PLIST "${SAMPLEPREP_GENERATED_INFO_PLIST}"
    )

    add_custom_command(TARGET sampleprep POST_BUILD
        COMMAND "${SAMPLEPREP_CODESIGN_EXECUTABLE}" --force --sign -
            "${CMAKE_CURRENT_BINARY_DIR}/${SAMPLEPREP_APP_NAME}.app"
        COMMENT "Ad-hoc signing macOS app bundle for local launch"
        VERBATIM
    )

    set(SAMPLEPREP_BUNDLE_SEARCH_DIRS
        ${SNDFILE_LIBRARY_DIRS}
        /opt/homebrew/lib
        /opt/homebrew/opt/libsndfile/lib
        /usr/local/lib
        /usr/local/opt/libsndfile/lib
    )
    list(REMOVE_DUPLICATES SAMPLEPREP_BUNDLE_SEARCH_DIRS)

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/StageMacOSBundle.cmake.in"
        "${SAMPLEPREP_GENERATED_DIR}/StageMacOSBundle.cmake"
        @ONLY
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/PrepareMacOSRelease.cmake.in"
        "${SAMPLEPREP_GENERATED_DIR}/PrepareMacOSRelease.cmake"
        @ONLY
    )

    add_custom_target(stage_macos_app
        COMMAND "${CMAKE_COMMAND}" -DSAMPLEPREP_CREATE_ZIP=OFF
            -P "${SAMPLEPREP_GENERATED_DIR}/StageMacOSBundle.cmake"
        DEPENDS sampleprep
        COMMENT "Staging a self-contained macOS app bundle"
        VERBATIM
    )

    add_custom_target(package_macos_release
        COMMAND "${CMAKE_COMMAND}" -P "${SAMPLEPREP_GENERATED_DIR}/PrepareMacOSRelease.cmake"
        DEPENDS stage_macos_app
        COMMENT "Preparing release artifacts and an uncompressed ZIP archive"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    add_executable(sampleprep_tests
        tests/sampleprep_tests.cpp
    )
    target_compile_definitions(sampleprep_tests PRIVATE SAMPLEPREP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(sampleprep_tests PRIVATE sampleprep_lib)
    add_test(NAME sampleprep_tests COMMAND sampleprep_tests)
endif()
