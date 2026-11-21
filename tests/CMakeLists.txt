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
target_link_libraries(test_telex PRIVATE xrnkey Catch2::Catch2WithMain)

add_executable(test_tone_placement test_tone_placement.cpp)
target_link_libraries(test_tone_placement PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_tone_placement PRIVATE
    XRNKEY_VI_SYLLABLES="${CMAKE_CURRENT_SOURCE_DIR}/../core/data/vi-syllables.txt")

add_executable(test_full_pipeline test_full_pipeline.cpp)
target_link_libraries(test_full_pipeline PRIVATE xrnkey Catch2::Catch2WithMain)

add_executable(test_instant_commit test_instant_commit.cpp)
target_link_libraries(test_instant_commit PRIVATE xrnkey Catch2::Catch2WithMain)

add_executable(test_real_words test_real_words.cpp)
target_link_libraries(test_real_words PRIVATE xrnkey Catch2::Catch2WithMain)

add_executable(test_corpus test_corpus.cpp)
target_link_libraries(test_corpus PRIVATE xrnkey Catch2::Catch2WithMain)

add_executable(test_user_dict test_user_dict.cpp)
target_link_libraries(test_user_dict PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_user_dict PRIVATE
    XRNKEY_TEST_DICT="${CMAKE_BINARY_DIR}/data/dict/vi.dict"
    XRNKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test_user_dict build_dict)

add_executable(test_lm test_lm.cpp)
target_link_libraries(test_lm PRIVATE xrnkey Catch2::Catch2WithMain)
target_include_directories(test_lm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)
target_compile_definitions(test_lm PRIVATE
    XRNKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm"
    XRNKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test_lm test_lm_models)

add_executable(test_restore test_restore.cpp)
target_link_libraries(test_restore PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_restore PRIVATE XRNKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm")
add_dependencies(test_restore test_lm_models)

add_executable(test_bilingual test_bilingual.cpp)
target_link_libraries(test_bilingual PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_bilingual PRIVATE XRNKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm")
add_dependencies(test_bilingual test_lm_models)

add_executable(test_learn test_learn.cpp)
target_link_libraries(test_learn PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_learn PRIVATE
    XRNKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm"
    XRNKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test_learn test_lm_models)

add_executable(test_typos test_typos.cpp)
target_link_libraries(test_typos PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_typos PRIVATE XRNKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm")
add_dependencies(test_typos test_lm_models)

add_executable(test_predict test_predict.cpp)
target_link_libraries(test_predict PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_predict PRIVATE XRNKEY_TEST_LM_DIR="${CMAKE_BINARY_DIR}/data/lm")
add_dependencies(test_predict test_lm_models)

add_executable(test_shortcuts test_shortcuts.cpp)
target_link_libraries(test_shortcuts PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_shortcuts PRIVATE XRNKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")

add_executable(test_emoji test_emoji.cpp)
target_link_libraries(test_emoji PRIVATE xrnkey Catch2::Catch2WithMain)
target_compile_definitions(test_emoji PRIVATE
    XRNKEY_EMOJI_INDEX="${CMAKE_CURRENT_SOURCE_DIR}/../data/emoji/emoji-index.tsv")

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
catch_discover_tests(test_predict)
catch_discover_tests(test_emoji)
