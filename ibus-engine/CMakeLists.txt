option(VIETKEY_BUILD_IBUS "Build IBus engine" ON)

if(VIETKEY_BUILD_IBUS)
    install(PROGRAMS vietkey-ibus DESTINATION bin)
    install(FILES vietkey_engine.py candidate_window.py config.py
            DESTINATION share/vietkey/ibus)
    install(FILES com.vietkey.IBus.xml
            DESTINATION share/dbus-1/interfaces)
    install(FILES com.vietkey.gschema.xml
            DESTINATION share/glib-2.0/schemas)
endif()
