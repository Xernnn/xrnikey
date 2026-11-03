cmake_minimum_required(VERSION 3.27)

set(XRNIKEY_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/xrnikey" CACHE PATH "Data install directory")

# Build dictionary at configure time
find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE)
    ## build_dict.sh defaults BUILD_DIR to a path relative to itself that
    ## assumes the CMake binary dir is literally named "build" — pass the
    ## real one explicitly, or any build dir with a different name (or
    ## location) silently gets no dictionary (masked by install(... OPTIONAL)
    ## below finding nothing at the expected path).
    add_custom_target(build_dict ALL
        COMMAND ${CMAKE_COMMAND} -E env "BUILD_DIR=${CMAKE_BINARY_DIR}/data/dict"
                ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/dict/build_dict.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dict
        COMMENT "Building Vietnamese dictionary"
    )
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ngram/
        DESTINATION share/xrnikey/ngram
        FILES_MATCHING PATTERN "*.arpa")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/emoji/
        DESTINATION share/xrnikey/emoji
        FILES_MATCHING PATTERN "*.json")

install(FILES dict/build_dict.sh dict/seed_wordlist.txt
        DESTINATION share/xrnikey/dict)

if(BASH_EXECUTABLE)
    install(FILES ${CMAKE_BINARY_DIR}/data/dict/vi.dict
            DESTINATION share/xrnikey/dict
            OPTIONAL)
endif()

install(PROGRAMS emoji/generate.py ngram/train.py
        DESTINATION share/xrnikey/scripts)

## fcitx5-addon/xrnikey.conf.in names its icon "xrnikey"; Fcitx5 looks that name up in the
## icon theme, so it has to land in hicolor (the fallback every theme inherits from).
install(FILES icons/xrnikey.svg
        DESTINATION share/icons/hicolor/scalable/apps)
