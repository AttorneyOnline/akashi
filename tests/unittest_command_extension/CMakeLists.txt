cmake_minimum_required(VERSION 3.16)
project(unittest_command_extension VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network Sql Test WebSockets)

add_executable(unittest_command_extension
    tst_unittest_command_extension.cpp
)
target_include_directories(unittest_command_extension PRIVATE
    .././../core
)

target_link_libraries(unittest_command_extension PRIVATE
    Qt::Core
    Qt::Network
    Qt::Sql
    Qt::Test
    Qt::WebSockets
)

if(UNIX)
    target_link_libraries(unittest_command_extension PRIVATE
        # Remove: L${CMAKE_CURRENT_SOURCE_DIR}/.././../bin/
        core
    )
endif()

if(WIN32)
    target_link_libraries(unittest_command_extension PRIVATE
        # Remove: L${CMAKE_CURRENT_SOURCE_DIR}/.././../bin/
        core
    )
endif()

if(coverage)
    target_link_libraries(unittest_command_extension PRIVATE
        gcov
    )
endif()

install(TARGETS unittest_command_extension
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
