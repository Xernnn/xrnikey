cmake_minimum_required(VERSION 3.27)

set(XRNKEY_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/xrnkey" CACHE PATH "Data install directory")

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
        DESTINATION share/xrnkey/ngram
        FILES_MATCHING PATTERN "*.arpa")

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/emoji/emoji-index.tsv
        DESTINATION share/xrnkey/emoji)

install(FILES dict/build_dict.sh dict/seed_wordlist.txt
        DESTINATION share/xrnkey/dict)

if(BASH_EXECUTABLE)
    install(FILES ${CMAKE_BINARY_DIR}/data/dict/vi.dict
            DESTINATION share/xrnkey/dict
            OPTIONAL)
endif()

install(PROGRAMS emoji/generate.py ngram/train.py
        DESTINATION share/xrnkey/scripts)

## fcitx5-addon/xrnkey.conf.in names its icon "xrnkey"; Fcitx5 looks that name up in the
## icon theme, so it has to land in hicolor (the fallback every theme inherits from).
install(FILES icons/xrnkey.svg
        DESTINATION share/icons/hicolor/scalable/apps)

## The small language models the tests use, trained from made-up text in
## lm/test (data/lm/README.md). They say nothing about quality; the real
## models are built by lm/build.py and installed from XRNKEY_LM_DIR.
if(XRNKEY_ENABLE_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(XK_TEST_LM_DIR "${CMAKE_BINARY_DIR}/data/lm")
    ## Each test model: its source files and its --min-count. vi leaves out
    ## words seen once, so the tests see words folded into <unk> as the real
    ## models have them. vi-general lacks the five rare words of vi-rare.txt,
    ## so the learning tests can show learning putting them right.
    set(XK_TEST_LM_SRC_vi lm/test/vi.txt lm/test/vi-rare.txt)
    set(XK_TEST_LM_SRC_vi-general lm/test/vi.txt)
    set(XK_TEST_LM_SRC_en lm/test/en.txt)
    set(XK_TEST_LM_MIN_vi 2)
    set(XK_TEST_LM_MIN_vi-general 2)
    set(XK_TEST_LM_MIN_en 1)
    set(XK_TEST_LM_OPTS_en --no-shape-index)
    set(XK_TEST_LM_FILES "")
    foreach(name vi vi-general en)
        set(srcs "")
        foreach(src ${XK_TEST_LM_SRC_${name}})
            list(APPEND srcs "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        endforeach()
        add_custom_command(
            OUTPUT "${XK_TEST_LM_DIR}/${name}.tok"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${XK_TEST_LM_DIR}"
            COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/lm/build.py" tokenize
                    --lib $<TARGET_FILE:xrnkey> "${XK_TEST_LM_DIR}/${name}.tok" ${srcs}
            DEPENDS xrnkey ${srcs}
                    "${CMAKE_CURRENT_SOURCE_DIR}/lm/build.py" "${CMAKE_CURRENT_SOURCE_DIR}/lm/xklm_text.py"
            COMMENT "Tokenizing the ${name} test corpus")
        add_custom_command(
            OUTPUT "${XK_TEST_LM_DIR}/${name}.xklm"
            COMMAND xk-lm-build --order 3 --min-count ${XK_TEST_LM_MIN_${name}} --prune 0,0
                    ${XK_TEST_LM_OPTS_${name}}
                    -o "${XK_TEST_LM_DIR}/${name}.xklm" "${XK_TEST_LM_DIR}/${name}.tok"
            DEPENDS xk-lm-build "${XK_TEST_LM_DIR}/${name}.tok"
            COMMENT "Training the ${name} test model")
        list(APPEND XK_TEST_LM_FILES "${XK_TEST_LM_DIR}/${name}.xklm")
    endforeach()
    add_custom_target(test_lm_models ALL DEPENDS ${XK_TEST_LM_FILES})

    add_test(NAME "lm: tokenizer"
             COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_CURRENT_SOURCE_DIR}/lm/test_xklm_text.py")
    set_tests_properties("lm: tokenizer" PROPERTIES
                         ENVIRONMENT "XRNKEY_LIB=$<TARGET_FILE:xrnkey>")
    ## The harness end to end on the test models: it must run and report, the
    ## numbers themselves mean nothing here.
    add_test(NAME "lm: xk-eval runs"
             COMMAND ${Python3_EXECUTABLE} -I "${CMAKE_SOURCE_DIR}/tools/xk-eval" --quick
                     --lib $<TARGET_FILE:xrnkey>
                     --vi "${XK_TEST_LM_DIR}/vi.xklm" --en "${XK_TEST_LM_DIR}/en.xklm")
endif()

## The real models, when built (lm/build.py train), are installed from here.
set(XRNKEY_LM_DIR "" CACHE PATH "Folder holding the trained vi.xklm and en.xklm to install")
if(XRNKEY_LM_DIR)
    install(FILES "${XRNKEY_LM_DIR}/vi.xklm" "${XRNKEY_LM_DIR}/en.xklm"
            DESTINATION share/xrnkey/lm)
endif()
