# Shared test tools, linked into every test as akashi::testtools: the
# header-only fakes plus compiled tools like the webhook receiver. The
# receiver needs the Qt HttpServer add-on; without it the library shrinks
# to the fakes and the tests that need the receiver are skipped.
if(TARGET Qt6::HttpServer)
    add_library(akashi_testtools STATIC
        include/testtools/fake_packet_context.h
        include/testtools/fake_transport.h
        include/testtools/webhook_receiver.h
        webhook_receiver.cpp
    )
    target_include_directories(akashi_testtools PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(akashi_testtools PUBLIC akashi_core Qt6::HttpServer)
else()
    message(STATUS "Qt6 HttpServer not found - test tools carry only the header-only fakes")
    add_library(akashi_testtools INTERFACE)
    target_include_directories(akashi_testtools INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(akashi_testtools INTERFACE akashi_core)
endif()
add_library(akashi::testtools ALIAS akashi_testtools)
