find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS QuickPhrase Notifications)

add_library(xrnkey_addon MODULE
    xrnkey_addon.cpp
    candidate_provider.cpp
    macro_table.cpp
    user_paths.cpp
)

target_link_libraries(xrnkey_addon
    PRIVATE Fcitx5::Core
    PRIVATE xrnkey
    ## Header-only interface target: brings in the notifications module's
    ## public header so the addon can show a tip when a setting is toggled.
    ## The addon declares it as an OPTIONAL dependency, so a system without
    ## the notifications addon still loads -- notifications() returns null.
    PRIVATE Fcitx5::Module::Notifications
)

target_include_directories(xrnkey_addon PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

target_compile_definitions(xrnkey_addon PRIVATE
    XRNKEY_DATA_DIR="${XRNKEY_DATA_DIR}"
)

## The addon is called xrnkey, like its input method, so Fcitx5 loads it from
## libxrnkey.so in its addon dir. The core library has the same file name in
## /usr/lib; they never meet, because /usr/lib/fcitx5 is not on the linker's
## search path and the addon asks for the core by its soname, libxrnkey.so.1.
## The target has its own name only because CMake's must be unique.
set_target_properties(xrnkey_addon PROPERTIES OUTPUT_NAME xrnkey)

## The addon library goes in Fcitx5's addon lib dir (e.g. /usr/lib/fcitx5,
## alongside libquickphrase.so, libclassicui.so, ...) — NOT under
## FCITX_INSTALL_PKGDATADIR/addon, which is reserved for the .conf metadata
## file below. Fcitx5 resolves the "Library=" key in that .conf against
## this directory.
install(TARGETS xrnkey_addon DESTINATION "${FCITX_INSTALL_ADDONDIR}")

configure_file(xrnkey-addon.conf.in addon/xrnkey.conf @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/addon/xrnkey.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

## The addon .conf above only tells Fcitx5 that a loadable addon exists; it
## does NOT make an input method selectable. Fcitx5's InputMethodManager
## discovers selectable input methods from a SEPARATE .conf under
## FCITX_INSTALL_PKGDATADIR/inputmethod — the addon parser reads only the
## [Addon] / [Addon/*] sections and silently ignores anything else, so an
## [InputMethod] stanza placed in the addon .conf has no effect at all.
## Without this file xrnkey loads as an addon but never appears in
## fcitx5-configtool's input method list.
configure_file(xrnkey.conf.in inputmethod/xrnkey.conf @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/inputmethod/xrnkey.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")

if(XRNKEY_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
