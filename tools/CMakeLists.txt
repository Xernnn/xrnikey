## Build-time tools: the language model builder. Nothing here is installed.
add_executable(xk-lm-build lm/xk-lm-build.cpp)
target_include_directories(xk-lm-build PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_compile_options(xk-lm-build PRIVATE -O2)
