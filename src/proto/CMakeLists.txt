# The AO2 protocol layer: the packet value type, codecs, packet handlers
# behind IPacketContext, and the registries they plug into. Nothing here
# may include core or world headers - the compiler enforces it, because
# only these include directories are visible. The objects ship inside the
# akashi_core library, so exported classes use the core export macro.
add_library(akashi_proto STATIC
    include/proto/ao2_protocol.h
    include/proto/area_music.h
    include/proto/area_music_messages.h
    include/proto/chat.h
    include/proto/chat_messages.h
    include/proto/client_profile.h
    include/proto/evidence.h
    include/proto/handshake.h
    include/proto/handshake_messages.h
    include/proto/ic.h
    include/proto/ic_message.h
    include/proto/message.h
    include/proto/moderation.h
    include/proto/moderation_messages.h
    include/proto/packet.h
    include/proto/packet_codec.h
    include/proto/packet_context.h
    include/proto/packet_handler.h
    include/proto/packet_registry.h
    include/proto/packet_service.h
    include/proto/text_utils.h
    include/proto/transport.h
    area_music.cpp
    chat.cpp
    evidence.cpp
    handshake.cpp
    ic.cpp
    moderation.cpp
    packet.cpp
    packet_codec.cpp
    packet_registry.cpp
    packet_service.cpp
    text_utils.cpp
)

target_include_directories(akashi_proto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(akashi_proto PUBLIC akashi_sdk Qt6::Network)
target_compile_definitions(akashi_proto PRIVATE akashi_core_EXPORTS)
set_target_properties(akashi_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)
