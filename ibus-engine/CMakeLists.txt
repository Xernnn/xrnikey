option(XRNIKEY_BUILD_IBUS "Build IBus engine" ON)

if(XRNIKEY_BUILD_IBUS)
    install(PROGRAMS xrnikey-ibus DESTINATION bin)
    install(FILES xrnikey_engine.py candidate_window.py config.py
            DESTINATION share/xrnikey/ibus)
    install(FILES com.xrnikey.IBus.xml
            DESTINATION share/dbus-1/interfaces)
    install(FILES com.xrnikey.gschema.xml
            DESTINATION share/glib-2.0/schemas)
endif()
