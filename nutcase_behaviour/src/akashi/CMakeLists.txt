qt_standard_project_setup()
find_package(Qt6 REQUIRED COMPONENTS Core)

qt_add_executable(akashi
    src/main.cpp
    src/pluginmanager.h src/pluginmanager.cpp
    src/server.h src/server.cpp
    src/softwareinformation.h
)

configure_file(
  cmake/softwareinformation.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/softwareinformation.h @ONLY
)

target_link_libraries(akashi PRIVATE
    Qt6::Core
    akashi::Core
    akashi::Network
    akashi::Protocol
)

if(WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET akashi PROPERTY WIN32_EXECUTABLE true)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_SOURCE_DIR}/data/icon/akashi.rc")
    target_sources(akashi PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
    set_target_properties(akashi PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
  endif()
endif()

set_target_properties(akashi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:$${CMAKE_SOURCE_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
)
