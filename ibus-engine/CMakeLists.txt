option(XRNKEY_BUILD_IBUS "Build IBus engine" ON)

if(XRNKEY_BUILD_IBUS)
    install(PROGRAMS xrnkey-ibus DESTINATION bin)
    install(FILES xrnkey_engine.py candidate_window.py config.py
            DESTINATION share/xrnkey/ibus)
    install(FILES com.xrnkey.IBus.xml
            DESTINATION share/dbus-1/interfaces)
    install(FILES com.xrnkey.gschema.xml
            DESTINATION share/glib-2.0/schemas)
endif()
