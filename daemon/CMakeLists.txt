cmake_minimum_required(VERSION 3.27)

add_executable(vkd
    main.cpp
    socket_server.cpp
)

target_compile_definitions(vkd PRIVATE
    VIETKEY_DATA_DIR="${VIETKEY_DATA_DIR}"
)

target_link_libraries(vkd PRIVATE vietkey pthread)

target_include_directories(vkd PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

install(TARGETS vkd DESTINATION bin)

configure_file(vkd.service.in vkd.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vkd.service
        DESTINATION lib/systemd/user)
