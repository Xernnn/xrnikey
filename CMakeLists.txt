cmake_minimum_required(VERSION 3.27)
project(xrnkey VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XRNKEY_BUILD_IBUS "Build IBus engine" ON)
option(XRNKEY_BUILD_FCITX5 "Build Fcitx5 addon" ON)
option(XRNKEY_ENABLE_TESTS "Build test suite" OFF)
## NOT option() — that declares a BOOL and takes its third argument as the
## initial VALUE, so `option(XRNKEY_DATA_DIR "/usr/share/xrnkey" "Data files
## install directory")` cached a BOOL of OFF and the daemon got compiled with
## XRNKEY_DATA_DIR="OFF", making every dict/ngram/emoji path load from ./OFF/.
## Must also be defined here at top level, not in data/CMakeLists.txt:
## add_subdirectory(fcitx5-addon), which compiles it in, runs before
## add_subdirectory(data).
set(XRNKEY_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/xrnkey"
    CACHE PATH "Data files install directory")

## Before any add_subdirectory(): the addon's tests live in fcitx5-addon/,
## the core's in tests/. Catch2 is found (or fetched) here, once, so its
## targets and its Catch.cmake module are visible to both directories.
if(XRNKEY_ENABLE_TESTS)
    enable_testing()
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
endif()

add_subdirectory(core)

if(XRNKEY_BUILD_FCITX5)
    find_package(Fcitx5Core QUIET)
    if(Fcitx5Core_FOUND)
        add_subdirectory(fcitx5-addon)
    else()
        message(STATUS "Fcitx5 not found, skipping fcitx5-addon")
    endif()
endif()

if(XRNKEY_BUILD_IBUS)
    add_subdirectory(ibus-engine)
endif()

add_subdirectory(cli-tools)
add_subdirectory(tools)
add_subdirectory(data)

if(TARGET test_fcitx5_addon AND TARGET build_dict)
    add_dependencies(test_fcitx5_addon build_dict)
endif()
if(TARGET test_fcitx5_addon AND TARGET test_lm_models)
    add_dependencies(test_fcitx5_addon test_lm_models)
endif()

if(XRNKEY_ENABLE_TESTS)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(FILES README.md ARCHITECTURE.md TROUBLESHOOTING.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        OPTIONAL)
