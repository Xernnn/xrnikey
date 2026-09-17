cmake_minimum_required(VERSION 3.27)
project(xrnikey VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XRNIKEY_BUILD_IBUS "Build IBus engine" ON)
option(XRNIKEY_BUILD_FCITX5 "Build Fcitx5 addon" ON)
option(XRNIKEY_BUILD_DAEMON "Build xkd daemon" ON)
option(XRNIKEY_ENABLE_TESTS "Build test suite" OFF)
## NOT option() — that declares a BOOL and takes its third argument as the
## initial VALUE, so `option(XRNIKEY_DATA_DIR "/usr/share/xrnikey" "Data files
## install directory")` cached a BOOL of OFF and the daemon got compiled with
## XRNIKEY_DATA_DIR="OFF", making every dict/ngram/emoji path load from ./OFF/.
## Must also be defined here at top level, not in data/CMakeLists.txt:
## add_subdirectory(daemon) runs before add_subdirectory(data).
set(XRNIKEY_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/xrnikey"
    CACHE PATH "Data files install directory")

add_subdirectory(core)

if(XRNIKEY_BUILD_DAEMON)
    add_subdirectory(daemon)
endif()

if(XRNIKEY_BUILD_FCITX5)
    find_package(Fcitx5Core QUIET)
    if(Fcitx5Core_FOUND)
        add_subdirectory(fcitx5-addon)
    else()
        message(STATUS "Fcitx5 not found, skipping fcitx5-addon")
    endif()
endif()

if(XRNIKEY_BUILD_IBUS)
    add_subdirectory(ibus-engine)
endif()

add_subdirectory(cli-tools)
add_subdirectory(data)

if(XRNIKEY_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(FILES README.md ARCHITECTURE.md TROUBLESHOOTING.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        OPTIONAL)
