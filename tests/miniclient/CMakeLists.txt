# A headless playtest client built from the real AO2-Client netcode.
# Run it by hand against a live server: ao2_miniclient [address] [port]
qt_add_executable(ao2_miniclient
    aopacket.cpp
    aopacket.h
    main.cpp
    websocketconnection.cpp
    websocketconnection.h
)
target_link_libraries(ao2_miniclient PRIVATE Qt6::Core Qt6::WebSockets)
target_compile_definitions(ao2_miniclient PRIVATE QT_NO_KEYWORDS)
