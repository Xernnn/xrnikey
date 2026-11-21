## The engine built for this machine, so the keyboard's JVM tests run the real core: the core,
## the JNI bridge against the JDK's jni.h, and the small test models trained from the made-up
## text in data/lm/test (as the core's own tests do). Gradle's buildHost task runs this.
cmake_minimum_required(VERSION 3.27)
project(xrnkey_host LANGUAGES CXX)

set(XRNKEY_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_subdirectory("${XRNKEY_REPO}/core" core)
add_subdirectory("${XRNKEY_REPO}/tools" tools)

find_package(JNI REQUIRED)
add_library(xrnkey_jni SHARED "${XRNKEY_REPO}/android/app/src/main/cpp/jni.cpp")
target_include_directories(xrnkey_jni PRIVATE ${JNI_INCLUDE_DIRS})
target_link_libraries(xrnkey_jni PRIVATE xrnkey)
set_target_properties(xrnkey_jni PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jni")
set_target_properties(xrnkey PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jni")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(LM "${CMAKE_BINARY_DIR}/lm")
set(SRC_vi "${XRNKEY_REPO}/data/lm/test/vi.txt" "${XRNKEY_REPO}/data/lm/test/vi-rare.txt")
set(SRC_en "${XRNKEY_REPO}/data/lm/test/en.txt")
set(OPT_vi --min-count 2)
set(OPT_en --min-count 1 --no-shape-index)
foreach(name vi en)
    add_custom_command(
        OUTPUT "${LM}/${name}.tok"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LM}"
        COMMAND ${Python3_EXECUTABLE} -I "${XRNKEY_REPO}/data/lm/build.py" tokenize
                --lib $<TARGET_FILE:xrnkey> "${LM}/${name}.tok" ${SRC_${name}}
        DEPENDS xrnkey ${SRC_${name}} "${XRNKEY_REPO}/data/lm/build.py" "${XRNKEY_REPO}/data/lm/xklm_text.py")
    add_custom_command(
        OUTPUT "${LM}/${name}.xklm"
        COMMAND xk-lm-build --order 3 ${OPT_${name}} --prune 0,0 -o "${LM}/${name}.xklm" "${LM}/${name}.tok"
        DEPENDS xk-lm-build "${LM}/${name}.tok")
endforeach()
add_custom_target(test_models ALL DEPENDS "${LM}/vi.xklm" "${LM}/en.xklm")
