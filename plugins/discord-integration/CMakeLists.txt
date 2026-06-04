include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)

akashi_add_plugin(akashi_discord
    SOURCES
        discord_plugin.h
        discord_plugin.cpp
        webhook_sender.h
        webhook_sender.cpp
        webhook_payloads.h
)
