find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS QuickPhrase)

add_library(vietkey5 MODULE
    vietkey5.cpp
    candidate_provider.cpp
)

target_link_libraries(vietkey5
    PRIVATE Fcitx5::Core
    PRIVATE vietkey
)

target_include_directories(vietkey5 PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

install(TARGETS vietkey5 DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

configure_file(vietkey5.conf.in vietkey5.conf @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vietkey5.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")

fcitx5_translate_desktop(vietkey5.conf.in vietkey5.conf)
