cmake_minimum_required(VERSION 3.27)
project(vietkey VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VIETKEY_BUILD_IBUS "Build IBus engine" ON)
option(VIETKEY_BUILD_FCITX5 "Build Fcitx5 addon" ON)
option(VIETKEY_BUILD_DAEMON "Build vkd daemon" ON)
option(VIETKEY_ENABLE_TESTS "Build test suite" OFF)
option(VIETKEY_DATA_DIR "/usr/share/vietkey" "Data files install directory")

add_subdirectory(core)

if(VIETKEY_BUILD_DAEMON)
    add_subdirectory(daemon)
endif()

if(VIETKEY_BUILD_FCITX5)
    find_package(Fcitx5Core QUIET)
    if(Fcitx5Core_FOUND)
        add_subdirectory(fcitx5-addon)
    else()
        message(STATUS "Fcitx5 not found, skipping fcitx5-addon")
    endif()
endif()

if(VIETKEY_BUILD_IBUS)
    add_subdirectory(ibus-engine)
endif()

add_subdirectory(cli-tools)
add_subdirectory(data)

if(VIETKEY_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(FILES README.md ARCHITECTURE.md TROUBLESHOOTING.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        OPTIONAL)
