cmake_minimum_required(VERSION 3.27)

find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(test_telex test_telex.cpp)
target_link_libraries(test_telex PRIVATE xrnikey Catch2::Catch2WithMain)

add_executable(test_tone_placement test_tone_placement.cpp)
target_link_libraries(test_tone_placement PRIVATE xrnikey Catch2::Catch2WithMain)
target_compile_definitions(test_tone_placement PRIVATE
    XRNIKEY_VI_SYLLABLES="${CMAKE_CURRENT_SOURCE_DIR}/../core/data/vi-syllables.txt")

add_executable(test_full_pipeline test_full_pipeline.cpp)
target_link_libraries(test_full_pipeline PRIVATE xrnikey Catch2::Catch2WithMain)

add_executable(test_instant_commit test_instant_commit.cpp)
target_link_libraries(test_instant_commit PRIVATE xrnikey Catch2::Catch2WithMain)

add_executable(test_real_words test_real_words.cpp)
target_link_libraries(test_real_words PRIVATE xrnikey Catch2::Catch2WithMain)

add_executable(test_corpus test_corpus.cpp)
target_link_libraries(test_corpus PRIVATE xrnikey Catch2::Catch2WithMain)

add_executable(test_user_dict test_user_dict.cpp)
target_link_libraries(test_user_dict PRIVATE xrnikey Catch2::Catch2WithMain)
target_compile_definitions(test_user_dict PRIVATE
    XRNIKEY_TEST_DICT="${CMAKE_BINARY_DIR}/data/dict/vi.dict"
    XRNIKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test_user_dict build_dict)

add_executable(test_lm test_lm.cpp)
target_link_libraries(test_lm PRIVATE xrnikey Catch2::Catch2WithMain)
target_include_directories(test_lm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)
target_compile_definitions(test_lm PRIVATE
    XRNIKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm"
    XRNIKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test_lm test_lm_models)

add_executable(test_restore test_restore.cpp)
target_link_libraries(test_restore PRIVATE xrnikey Catch2::Catch2WithMain)
target_compile_definitions(test_restore PRIVATE XRNIKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm")
add_dependencies(test_restore test_lm_models)

add_executable(test_bilingual test_bilingual.cpp)
target_link_libraries(test_bilingual PRIVATE xrnikey Catch2::Catch2WithMain)
target_compile_definitions(test_bilingual PRIVATE XRNIKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm")
add_dependencies(test_bilingual test_lm_models)

add_executable(test_learn test_learn.cpp)
target_link_libraries(test_learn PRIVATE xrnikey Catch2::Catch2WithMain)
target_compile_definitions(test_learn PRIVATE
    XRNIKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm"
    XRNIKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test_learn test_lm_models)

add_executable(test_typos test_typos.cpp)
target_link_libraries(test_typos PRIVATE xrnikey Catch2::Catch2WithMain)
target_compile_definitions(test_typos PRIVATE XRNIKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm")
add_dependencies(test_typos test_lm_models)

add_executable(test_shortcuts test_shortcuts.cpp)
target_link_libraries(test_shortcuts PRIVATE xrnikey Catch2::Catch2WithMain)
target_compile_definitions(test_shortcuts PRIVATE XRNIKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")

include(CTest)
include(Catch)
catch_discover_tests(test_telex)
catch_discover_tests(test_tone_placement)
catch_discover_tests(test_full_pipeline)
catch_discover_tests(test_instant_commit)
catch_discover_tests(test_real_words)
catch_discover_tests(test_user_dict)
catch_discover_tests(test_corpus)
catch_discover_tests(test_lm)
catch_discover_tests(test_restore)
catch_discover_tests(test_bilingual)
catch_discover_tests(test_learn)
catch_discover_tests(test_typos)
catch_discover_tests(test_shortcuts)
