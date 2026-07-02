cmake_minimum_required(VERSION 3.27)
project(vietkey_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(vietkey SHARED
    vietkey.cpp
    utf8_utils.cpp
    telex.cpp
    vni.cpp
    tone_placement.cpp
    syllable_validator.cpp
    dict.cpp
    ngram.cpp
    autocorrect.cpp
    bilingual_switch.cpp
    emoji.cpp
)

target_include_directories(vietkey PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/vietkey>
)

set_target_properties(vietkey PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER vietkey.h
)

target_link_libraries(vietkey PRIVATE pthread)

install(TARGETS vietkey
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/vietkey
)
