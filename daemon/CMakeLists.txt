cmake_minimum_required(VERSION 3.27)

add_executable(xkd
    main.cpp
    socket_server.cpp
)

target_compile_definitions(xkd PRIVATE
    XRNIKEY_DATA_DIR="${XRNIKEY_DATA_DIR}"
)

target_link_libraries(xkd PRIVATE xrnikey pthread)

target_include_directories(xkd PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

install(TARGETS xkd DESTINATION bin)

configure_file(xkd.service.in xkd.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xkd.service
        DESTINATION lib/systemd/user)
