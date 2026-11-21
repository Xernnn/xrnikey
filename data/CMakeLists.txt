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

## The small language models the tests use, trained from made-up text in
## lm/test (data/lm/README.md). They say nothing about quality; the real
## models are built by lm/build.py and installed from XRNIKEY_LM_DIR.
if(XRNIKEY_ENABLE_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(XK_TEST_LM_DIR "${CMAKE_BINARY_DIR}/data/lm")
    foreach(lang vi en)
        add_custom_command(
            OUTPUT "${XK_TEST_LM_DIR}/${lang}.tok"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${XK_TEST_LM_DIR}"
            COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/lm/build.py" tokenize
                    --lib $<TARGET_FILE:xrnikey>
                    "${CMAKE_CURRENT_SOURCE_DIR}/lm/test/${lang}.txt" "${XK_TEST_LM_DIR}/${lang}.tok"
            DEPENDS xrnikey "${CMAKE_CURRENT_SOURCE_DIR}/lm/test/${lang}.txt"
                    "${CMAKE_CURRENT_SOURCE_DIR}/lm/build.py" "${CMAKE_CURRENT_SOURCE_DIR}/lm/xklm_text.py"
            COMMENT "Tokenizing the ${lang} test corpus")
        add_custom_command(
            OUTPUT "${XK_TEST_LM_DIR}/${lang}.xklm"
            COMMAND xk-lm-build --order 3 --min-count 1 --prune 0,0
                    -o "${XK_TEST_LM_DIR}/${lang}.xklm" "${XK_TEST_LM_DIR}/${lang}.tok"
            DEPENDS xk-lm-build "${XK_TEST_LM_DIR}/${lang}.tok"
            COMMENT "Training the ${lang} test model")
    endforeach()
    add_custom_target(test_lm_models ALL
        DEPENDS "${XK_TEST_LM_DIR}/vi.xklm" "${XK_TEST_LM_DIR}/en.xklm")

    add_test(NAME "lm: tokenizer"
             COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/lm/test_xklm_text.py")
    set_tests_properties("lm: tokenizer" PROPERTIES
                         ENVIRONMENT "XRNIKEY_LIB=$<TARGET_FILE:xrnikey>")
    ## The harness end to end on the test models: it must run and report, the
    ## numbers themselves mean nothing here.
    add_test(NAME "lm: xk-eval runs"
             COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_SOURCE_DIR}/tools/xk-eval" --quick
                     --lib $<TARGET_FILE:xrnikey>
                     --vi "${XK_TEST_LM_DIR}/vi.xklm" --en "${XK_TEST_LM_DIR}/en.xklm")
endif()

## The real models, when built (lm/build.py train), are installed from here.
set(XRNIKEY_LM_DIR "" CACHE PATH "Folder holding the trained vi.xklm and en.xklm to install")
if(XRNIKEY_LM_DIR)
    install(FILES "${XRNIKEY_LM_DIR}/vi.xklm" "${XRNIKEY_LM_DIR}/en.xklm"
            DESTINATION share/xrnikey/lm)
endif()
