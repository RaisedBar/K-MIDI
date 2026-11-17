# K-MIDI: makes Kontrol S MK3 MIDI templates (.kmt) from instrument definitions.
#
#   Windows: cmake --preset windows-vs2022   -> build/windows-vs2022/KMidi.sln
#   macOS:   cmake --preset macos-xcode      -> build/macos-xcode/KMidi.xcodeproj
#
# Dependencies (wxWidgets, nlohmann-json, toml++) come from vcpkg in manifest mode (vcpkg.json).

cmake_minimum_required(VERSION 3.21)

project(KMidi VERSION 0.1.0 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

find_package(wxWidgets CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

if(MSVC)
    set(KMIDI_WARNINGS /W4 /utf-8 /permissive- /Zc:__cplusplus)
else()
    set(KMIDI_WARNINGS -Wall -Wextra -Wno-unused-parameter)
endif()

# ---- Core library (no GUI) -------------------------------------------------------------
add_library(KMidiCore STATIC
    src/core/Instrument.cpp   src/core/Instrument.h
    src/core/InsLoader.cpp    src/core/InsLoader.h
    src/core/TomlLoader.cpp   src/core/TomlLoader.h
    src/core/Reabank.cpp      src/core/Reabank.h
    src/core/Library.cpp      src/core/Library.h
    src/core/Layout.cpp       src/core/Layout.h
    src/core/Kmt.cpp          src/core/Kmt.h
    src/core/History.cpp      src/core/History.h
)
target_include_directories(KMidiCore PUBLIC src/core)
target_link_libraries(KMidiCore PUBLIC nlohmann_json::nlohmann_json tomlplusplus::tomlplusplus)
target_compile_options(KMidiCore PRIVATE ${KMIDI_WARNINGS})
if(WIN32)
    target_compile_definitions(KMidiCore PUBLIC NOMINMAX UNICODE _UNICODE)
endif()

# ---- Tests ----------------------------------------------------------------------------
enable_testing()
add_executable(KMidiTests tests/Tests.cpp)
target_link_libraries(KMidiTests PRIVATE KMidiCore)
target_compile_options(KMidiTests PRIVATE ${KMIDI_WARNINGS})
target_compile_definitions(KMidiTests PRIVATE
    KMIDI_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    KMIDI_SAMPLES="${CMAKE_CURRENT_SOURCE_DIR}/samples")
add_test(NAME core COMMAND KMidiTests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# ---- GUI application ------------------------------------------------------------------
set(APP_SOURCES
    src/app/App.cpp
    src/app/MainFrame.cpp    src/app/MainFrame.h
    src/app/SlotDialog.cpp   src/app/SlotDialog.h
    src/app/TableView.cpp    src/app/TableView.h
    src/app/Announce.h
)
# Screen reader helpers for each platform
if(WIN32)
    list(APPEND APP_SOURCES src/app/Announce_msw.cpp src/app/Accessibility_msw.cpp src/app/KMidi.rc)
elseif(APPLE)
    list(APPEND APP_SOURCES src/app/Announce_mac.mm)
else()
    list(APPEND APP_SOURCES src/app/Announce_generic.cpp)
endif()

# The lists use wxDataViewCtrl on macOS, where it is a native table VoiceOver reads. This option uses it on
# Windows too, to check that code there; the lists then aren't accessible on Windows.
option(KMIDI_TABLE_USE_DATAVIEW "Use the macOS list implementation on Windows too (testing only)" OFF)

add_executable(KMidi WIN32 MACOSX_BUNDLE ${APP_SOURCES})
target_link_libraries(KMidi PRIVATE KMidiCore wx::core wx::base)
# The program is called K-MIDI (K-MIDI.exe, K-MIDI.app); the target is KMidi, as CMake target names are simplest without a hyphen
set_target_properties(KMidi PROPERTIES OUTPUT_NAME "K-MIDI")
target_compile_options(KMidi PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${KMIDI_WARNINGS}>)
if(KMIDI_TABLE_USE_DATAVIEW)
    target_compile_definitions(KMidi PRIVATE KMIDI_TABLE_USE_DATAVIEW)
endif()
if(WIN32)
    target_link_libraries(KMidi PRIVATE uiautomationcore oleacc)
    # The application manifest (DPI awareness, common controls v6) comes from wx/msw/wx.rc.
    target_link_options(KMidi PRIVATE /MANIFEST:NO)
elseif(APPLE)
    target_link_libraries(KMidi PRIVATE "-framework AppKit")
    set_target_properties(KMidi PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "K-MIDI"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.k-midi"   # change to your own before distributing
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"   # ad-hoc signing for local builds
    )
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT KMidi)

# ---- Windows installer ----------------------------------------------------------------
# `cmake --install` puts K-MIDI.exe in one folder with the DLLs it uses (from vcpkg) and the Visual C++ runtime,
# and installer/make-installer.ps1 turns that folder into a setup program with Inno Setup.
if(WIN32)
    install(TARGETS KMidi
        RUNTIME_DEPENDENCIES
            DIRECTORIES $<TARGET_FILE_DIR:KMidi>
            PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
            POST_EXCLUDE_REGEXES "[/\\\\][Ss]ystem32[/\\\\]"
        RUNTIME DESTINATION .
    )
    # The Visual C++ runtime goes next to the program, so users don't have to install it separately.
    # The Universal CRT is part of Windows 10 and later.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)

    configure_file(installer/K-MIDI.iss.in ${CMAKE_BINARY_DIR}/K-MIDI.iss @ONLY)
endif()
