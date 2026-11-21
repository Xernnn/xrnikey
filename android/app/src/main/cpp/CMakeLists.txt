## The phone's native code: the shared core (../core, one copy of the sources for the phone
## and the laptop) and the JNI bridge the keyboard calls it through.
cmake_minimum_required(VERSION 3.27)
project(xrnkey_android LANGUAGES CXX)

set(XRNKEY_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
add_subdirectory("${XRNKEY_REPO}/core" core)

add_library(xrnkey_jni SHARED jni.cpp)
target_link_libraries(xrnkey_jni PRIVATE xrnkey)
target_compile_options(xrnkey_jni PRIVATE -Wall -Wextra)
