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
target_link_libraries(test_telex PRIVATE vietkey Catch2::Catch2WithMain)

add_executable(test_tone_placement test_tone_placement.cpp)
target_link_libraries(test_tone_placement PRIVATE vietkey Catch2::Catch2WithMain)

add_executable(test_full_pipeline test_full_pipeline.cpp)
target_link_libraries(test_full_pipeline PRIVATE vietkey Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(test_telex)
catch_discover_tests(test_tone_placement)
catch_discover_tests(test_full_pipeline)
