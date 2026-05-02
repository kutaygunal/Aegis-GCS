cmake_minimum_required(VERSION 3.20)
project(AegisGCS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ──────────────────────────────────────────────────────────
option(AEGIS_BUILD_TESTS "Build unit and integration tests" ON)
option(AEGIS_BUILD_PLUGINS "Build example plugins" ON)
option(AEGIS_ENABLE_LTO "Enable link-time optimization for Release builds" ON)

# ── Qt6 ──────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 QUIET COMPONENTS Core Widgets Network WebEngineWidgets Qml)

if(NOT Qt6_FOUND)
    message(FATAL_ERROR 
"Qt6 not found.

Please install Qt6 and set the path before running CMake:
    Windows (PowerShell):
        cmake -B build -S . -DCMAKE_PREFIX_PATH='C:/Qt/6.8.2/msvc2022_64'
    Or install Qt6 via aqtinstall:
        pip install aqtinstall
        aqt install-qt windows desktop 6.8.2 win64_msvc2022_64 --outputdir C:/Qt
        cmake -B build -S . -DCMAKE_PREFIX_PATH='C:/Qt/6.8.2/msvc2022_64'

    Linux:
        sudo apt install qt6-base-dev qt6-webengine-dev libqt6widgets6
        cmake -B build -S .

    macOS:
        brew install qt@6
        cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
")
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    WebEngineWidgets
    Qml
)

# ── Compiler flags ───────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX- /permissive- /Zc:__cplusplus /MP)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND AEGIS_ENABLE_LTO)
        add_compile_options(-flto)
        add_link_options(-flto)
    endif()
endif()

# ── Core library ──────────────────────────────────────────────────────
add_subdirectory(src/core)
add_subdirectory(src/telemetry)
add_subdirectory(src/ui)
add_subdirectory(src/utils)

# ── Executable ──────────────────────────────────────────────────────
add_executable(aegis
    src/main.cpp
    src/app/application.cpp
    src/app/application.hpp
    resources/resources.qrc
)

target_link_libraries(aegis PRIVATE
    aegis_core
    aegis_telemetry
    aegis_ui
    aegis_utils
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
)

set_target_properties(aegis PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)

# ── Plugins ─────────────────────────────────────────────────────────
if(AEGIS_BUILD_PLUGINS)
    add_subdirectory(src/plugins/telemetry_hud)
    add_subdirectory(src/plugins/mission_editor)
    add_subdirectory(src/plugins/alert_console)
endif()

# ── Tests ───────────────────────────────────────────────────────────
if(AEGIS_BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(WARNING "GoogleTest not found. Tests will not be built. "
                        "Install it: vcpkg install gtest, or "
                        "https://github.com/google/googletest")
        set(AEGIS_BUILD_TESTS OFF)
    else()
        add_subdirectory(tests)
    endif()
endif()

# ── Install ──────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS aegis
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY config/ DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/aegis)
