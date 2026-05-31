include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)

akashi_add_plugin(akashi_sql_logger
    SOURCES
        sql_logger_plugin.h
        sql_logger_plugin.cpp
        writer_sql.h
        writer_sql.cpp
)

target_link_libraries(akashi_sql_logger PRIVATE Qt6::Sql)
