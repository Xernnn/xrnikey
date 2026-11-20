install(PROGRAMS xk-setup.sh DESTINATION bin RENAME xk-setup)
install(PROGRAMS xk-diagnose.sh DESTINATION bin RENAME xk-diagnose)
install(PROGRAMS xk-dict.py DESTINATION bin RENAME xk-dict)
install(PROGRAMS xk-macro.py DESTINATION bin RENAME xk-macro)

if(XRNIKEY_ENABLE_TESTS)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME "cli: xk-macro"
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_xk_macro.py)
        add_test(NAME "cli: xk-setup"
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_xk_setup.py)
    endif()
endif()
