cmake_minimum_required(VERSION 3.27)
project(xrnikey_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(xrnikey SHARED
    xrnikey.cpp
    utf8_utils.cpp
    telex.cpp
    vni.cpp
    tone_placement.cpp
    syllable_validator.cpp
    dict.cpp
    ngram.cpp
    lm.cpp
    restore.cpp
    score.cpp
    learn.cpp
    keys.cpp
    shortcuts.cpp
    autocorrect.cpp
    bilingual_switch.cpp
    emoji.cpp
    word.cpp
    lexicon.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/lexicon_data.cpp
)

# The word lists auto-restore consults are compiled in, so the engine needs no
# data files at run time (data/README.md says where they come from).
set(XK_ENGLISH_FILE english.txt)
set(XK_VI_FILE vi-syllables.txt)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/data/${XK_ENGLISH_FILE} XK_ENGLISH_WORDS)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/data/${XK_VI_FILE} XK_VI_SYLLABLES)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/data/${XK_ENGLISH_FILE}
    ${CMAKE_CURRENT_SOURCE_DIR}/data/${XK_VI_FILE})
configure_file(lexicon_data.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/lexicon_data.cpp @ONLY)

target_include_directories(xrnikey PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/xrnikey>
)

set_target_properties(xrnikey PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER xrnikey.h
)

find_package(Threads REQUIRED)
target_link_libraries(xrnikey PRIVATE Threads::Threads)

install(TARGETS xrnikey
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/xrnikey
)
