cmake_minimum_required(VERSION 3.19)
project(akashi VERSION 1.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network WebSockets Sql)
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets Sql)

qt_standard_project_setup()
qt_add_executable(akashi
    src/main.cpp
)

target_link_libraries(akashi PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Qt6::WebSockets
)

target_include_directories(akashi PRIVATE src src/logger src/network src/packet)

set_target_properties(akashi PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET akashi PROPERTY WIN32_EXECUTABLE true)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/data/logo-client.rc")
    target_sources(akashi PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
  endif()
endif()

set_target_properties(akashi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>)
