## Build-time tools: the language model builder. Nothing here is installed.
add_executable(xk-lm-build lm/xk-lm-build.cpp)
target_include_directories(xk-lm-build PRIVATE ${CMAKE_SOURCE_DIR}/core)
## The shape index in each model is worked out by the core itself.
target_link_libraries(xk-lm-build PRIVATE xrnkey)
target_compile_options(xk-lm-build PRIVATE -O2)

## What the engine costs per call, with real models (xk-bench VI.xklm EN.xklm).
add_executable(xk-bench bench/xk-bench.cpp)
target_link_libraries(xk-bench PRIVATE xrnkey)
target_compile_options(xk-bench PRIVATE -O2)
