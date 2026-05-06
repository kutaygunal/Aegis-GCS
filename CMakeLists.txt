cmake_minimum_required(VERSION 3.20)
project(AegisGCS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ──────────────────────────────────────────────────────────
option(AEGIS_BUILD_TESTS "Build unit and integration tests" ON)
option(AEGIS_BUILD_PLUGINS "Build example plugins" ON)
option(AEGIS_ENABLE_LTO "Enable link-time optimization for Release builds" ON)
option(AEGIS_CI_BUILD "Enable CI-specific flags (warnings-as-errors, sanitizers)" OFF)

# ── Qt6 ──────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Qml Concurrent
             OPTIONAL_COMPONENTS WebEngineWidgets)

if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 Core/Widgets/Network/Qml not found. Set CMAKE_PREFIX_PATH to your Qt6 installation.")
endif()
if(NOT TARGET Qt6::WebEngineWidgets)
    message(WARNING "Qt6::WebEngineWidgets not found. Native Qt map plugin will be used.")
endif()

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

# ── CI-specific flags ────────────────────────────────────────────────
if(AEGIS_CI_BUILD)
    message(STATUS "CI build enabled: warnings-as-errors + sanitizers")
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
        # AddressSanitizer on Linux (GCC/Clang)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
            add_link_options(-fsanitize=address)
            add_compile_definitions(ASAN_ENABLED)
        endif()
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
)
if(TARGET Qt6::WebEngineWidgets)
    target_link_libraries(aegis PRIVATE Qt6::WebEngineWidgets)
endif()

set_target_properties(aegis PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)

# ── Automatic deployment (Qt DLLs) ────────────────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        PATHS "${Qt6Core_DIR}/../../../bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET aegis POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    "$<TARGET_FILE:aegis>"
                    --no-translations
                    --no-compiler-runtime
            COMMENT "Deploying Qt runtime dependencies..."
        )
    endif()
endif()

# ── Plugins ─────────────────────────────────────────────────────────
if(AEGIS_BUILD_PLUGINS)
    add_subdirectory(src/plugins/telemetry_hud)
    add_subdirectory(src/plugins/mission_editor)
    add_subdirectory(src/plugins/alert_console)
    add_subdirectory(src/plugins/map_view)
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
