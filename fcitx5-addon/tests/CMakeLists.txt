## Drives the built addon through a real Fcitx5 Instance, in process: the
## same way fcitx5's own engines (unikey, chinese-addons) test theirs. No
## D-Bus, display or user config is touched -- setupTestingEnvironment()
## points every Fcitx5 path at the build tree, and the test gives the addon
## throwaway XDG dirs for the files it reads itself.

## Catch2 is resolved once at the top level, for every test directory.

## Fcitx5 finds an addon by its .conf under <datadir>/addon and an input
## method under <datadir>/inputmethod; lay the generated ones out that way.
set(XRNKEY_TEST_FCITX_DATA "${CMAKE_CURRENT_BINARY_DIR}/fcitx5")
configure_file(../xrnkey-addon.conf.in "${XRNKEY_TEST_FCITX_DATA}/addon/xrnkey.conf" @ONLY)
configure_file(../xrnkey.conf.in "${XRNKEY_TEST_FCITX_DATA}/inputmethod/xrnkey.conf" @ONLY)

add_executable(test_fcitx5_addon test_fcitx5_addon.cpp)
target_link_libraries(test_fcitx5_addon PRIVATE Fcitx5::Core Catch2::Catch2)
target_include_directories(test_fcitx5_addon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(test_fcitx5_addon PRIVATE
    XRNKEY_TEST_ADDON_DIR="$<TARGET_FILE_DIR:xrnkey_addon>"
    XRNKEY_TEST_FCITX_DATA="${XRNKEY_TEST_FCITX_DATA}"
    XRNKEY_TEST_DATA_DIR="${CMAKE_BINARY_DIR}/data"
    XRNKEY_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
## (The dependency on the dictionary is added at the top level, after
## data/ has defined build_dict.)
add_dependencies(test_fcitx5_addon xrnkey_addon)

include(CTest)
include(Catch)
catch_discover_tests(test_fcitx5_addon TEST_PREFIX "addon: ")

## The same addon in the real fcitx5 binary, typed into over D-Bus like the
## GTK/Qt modules do (skipped where fcitx5, dbus-daemon or python3-gi is
## missing).
add_test(NAME "addon: real fcitx5 over D-Bus"
         COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/dbus_session.sh
                 $<TARGET_FILE_DIR:xrnkey_addon> ${CMAKE_CURRENT_BINARY_DIR}
                 ${CMAKE_BINARY_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}
                 ${CMAKE_CURRENT_SOURCE_DIR}/dbus_driver.py)
set_tests_properties("addon: real fcitx5 over D-Bus" PROPERTIES
                     SKIP_RETURN_CODE 77 TIMEOUT 90)
