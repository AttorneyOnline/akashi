# The world model: areas, floors, the jukebox, evidence, testimony, rules
# and the config loading for all of it. It may use protocol value types
# but never builds packets or touches core - only these include
# directories are visible, so the compiler enforces the boundary. The
# objects ship inside the akashi_core library, so exported classes use
# the core export macro.
add_library(akashi_world STATIC
    include/world/area.h
    include/world/area_settings.h
    include/world/config_loading.h
    include/world/evidence_store.h
    include/world/floodguard.h
    include/world/floor.h
    include/world/jukebox.h
    include/world/rule_registry.h
    include/world/testimony_recorder.h
    include/world/world.h
    area.cpp
    config_loading.cpp
    evidence_store.cpp
    floodguard.cpp
    jukebox.cpp
    rule_registry.cpp
    testimony_recorder.cpp
    world.cpp
)

target_include_directories(akashi_world PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(akashi_world PUBLIC akashi_proto)
target_compile_definitions(akashi_world PRIVATE akashi_core_EXPORTS)
set_target_properties(akashi_world PROPERTIES POSITION_INDEPENDENT_CODE ON)
