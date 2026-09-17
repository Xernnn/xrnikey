find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS QuickPhrase Notifications)

add_library(xrnikey5 MODULE
    xrnikey5.cpp
    candidate_provider.cpp
)

target_link_libraries(xrnikey5
    PRIVATE Fcitx5::Core
    PRIVATE xrnikey
    ## Header-only interface target: brings in the notifications module's
    ## public header so the addon can show a tip when a setting is toggled.
    ## The addon declares it as an OPTIONAL dependency, so a system without
    ## the notifications addon still loads -- notifications() returns null.
    PRIVATE Fcitx5::Module::Notifications
)

target_include_directories(xrnikey5 PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

target_compile_definitions(xrnikey5 PRIVATE
    XRNIKEY_DATA_DIR="${XRNIKEY_DATA_DIR}"
)

## The addon library goes in Fcitx5's addon lib dir (e.g. /usr/lib/fcitx5,
## alongside libquickphrase.so, libclassicui.so, ...) — NOT under
## FCITX_INSTALL_PKGDATADIR/addon, which is reserved for the .conf metadata
## file below. Fcitx5 resolves the "Library=" key in that .conf against
## this directory.
install(TARGETS xrnikey5 DESTINATION "${FCITX_INSTALL_ADDONDIR}")

configure_file(xrnikey5.conf.in xrnikey5.conf @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xrnikey5.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

## The addon .conf above only tells Fcitx5 that a loadable addon exists; it
## does NOT make an input method selectable. Fcitx5's InputMethodManager
## discovers selectable input methods from a SEPARATE .conf under
## FCITX_INSTALL_PKGDATADIR/inputmethod — the addon parser reads only the
## [Addon] / [Addon/*] sections and silently ignores anything else, so an
## [InputMethod] stanza placed in the addon .conf has no effect at all.
## Without this file Xrnikey loads as an addon but never appears in
## fcitx5-configtool's input method list.
configure_file(xrnikey.conf.in xrnikey.conf @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xrnikey.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
