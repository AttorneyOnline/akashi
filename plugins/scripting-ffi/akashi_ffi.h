/* AI-generated: written by Claude. */
/*
 * The flat C surface scripting hosts build on. This header is deliberately
 * self-contained: no Qt types, UTF-8 strings passed as pointer plus explicit
 * length, callbacks as plain function pointers with a userdata slot.
 *
 * ABI rules: the table only ever GROWS. New functions are appended at the
 * end, existing entries never change signature or position, and abi_version
 * counts up with every addition, so a host built against an older header
 * keeps working against a newer table.
 *
 * Threading: everything runs on the server's main thread. Returned string
 * pointers stay valid until the next FFI call, so hosts copy immediately.
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AKASHI_FFI_ABI_VERSION 2

/* Valid only for the duration of a command callback. */
typedef struct AkashiCommandContext AkashiCommandContext;

/* A command handler. f_argv holds f_argc UTF-8, NUL-terminated arguments. */
typedef void (*AkashiCommandFn)(void *f_userdata, AkashiCommandContext *f_context,
                                int f_argc, const char *const *f_argv);

/* Carries a text filter's rewritten text back; valid during the callback. */
typedef struct AkashiTextResult AkashiTextResult;

/* A text filter. Return 0 to drop the message entirely; return 1 to let it
 * through, rewritten if text_result_set was called, unchanged otherwise. */
typedef int (*AkashiTextFilterFn)(void *f_userdata, const char *f_text, size_t f_text_length,
                                  AkashiTextResult *f_result);

/* An event handler. The payload arrives as f_count key/value string pairs. */
typedef void (*AkashiEventFn)(void *f_userdata, int f_count,
                              const char *const *f_keys, const char *const *f_values);

typedef struct AkashiFfi
{
    int abi_version;

    /* Writes one line to the server log. */
    void (*log_info)(const char *f_text, size_t f_text_length);

    /* Registers a chat command. f_permission may be empty for a free
     * command. Commands registered under an owner id are removed together
     * by unregister_owner or when that plugin unloads. Returns 1 on
     * success, 0 when the name is taken or the arguments are invalid. */
    int (*register_command)(const char *f_name, size_t f_name_length,
                            const char *f_usage, size_t f_usage_length,
                            const char *f_description, size_t f_description_length,
                            const char *f_permission, size_t f_permission_length,
                            int f_min_args,
                            AkashiCommandFn f_handler, void *f_userdata,
                            const char *f_owner_id, size_t f_owner_id_length);

    /* Removes everything the owner registered across all registries. */
    void (*unregister_owner)(const char *f_owner_id, size_t f_owner_id_length);

    /* Replies to the invoker of the running command. */
    void (*reply)(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length);

    /* Replies to everyone in the invoker's area. */
    void (*reply_to_area)(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length);

    /* The invoker's client id. */
    int (*client_id)(AkashiCommandContext *f_context);

    /* --- Added in ABI version 2 --- */

    /* Who invoked the running command and where they stand. */
    const char *(*context_player_name)(AkashiCommandContext *f_context, size_t *f_out_length);
    const char *(*context_character)(AkashiCommandContext *f_context, size_t *f_out_length);
    const char *(*context_area_name)(AkashiCommandContext *f_context, size_t *f_out_length);
    int (*context_area_id)(AkashiCommandContext *f_context);
    int (*context_is_authenticated)(AkashiCommandContext *f_context);
    int (*context_can_perform)(AkashiCommandContext *f_context,
                               const char *f_permission, size_t f_permission_length);

    /* Target verbs resolve the command argument at f_argument_index
     * (0-based) as a client id. They return 0 (or -1 for the id) when the
     * argument names nobody. */
    int (*target_client_id)(AkashiCommandContext *f_context, int f_argument_index);
    int (*target_reply)(AkashiCommandContext *f_context, int f_argument_index,
                        const char *f_text, size_t f_text_length);
    int (*target_has_sanction)(AkashiCommandContext *f_context, int f_argument_index,
                               const char *f_sanction_id, size_t f_sanction_id_length);
    int (*target_set_sanction)(AkashiCommandContext *f_context, int f_argument_index,
                               const char *f_sanction_id, size_t f_sanction_id_length,
                               int f_active);
    int (*target_change_area)(AkashiCommandContext *f_context, int f_argument_index,
                              int f_area_id);

    /* Registers an IC text filter. A filter with f_always_active 0 runs for
     * clients whose sanction set holds f_id, like the built-in curses. */
    int (*register_text_filter)(const char *f_id, size_t f_id_length,
                                int f_order, int f_always_active,
                                AkashiTextFilterFn f_filter, void *f_userdata,
                                const char *f_owner_id, size_t f_owner_id_length);

    /* Hands the rewritten text back from inside a text filter callback. */
    void (*text_result_set)(AkashiTextResult *f_result, const char *f_text, size_t f_text_length);

    /* Subscribes to a named event, either a core event (modcall, ban_issued,
     * kick_issued, ic_message, ooc_message, player_joined_area,
     * player_left_area, area_changed, music_changed, evidence_presented,
     * command_executed, config_reloaded) or any custom name published by a
     * plugin. Handlers run after the event happened. Returns 1 on success. */
    int (*subscribe_event)(const char *f_name, size_t f_name_length,
                           AkashiEventFn f_handler, void *f_userdata,
                           const char *f_owner_id, size_t f_owner_id_length);

    /* Publishes a custom event to every subscriber of the name. */
    void (*publish_event)(const char *f_name, size_t f_name_length, int f_count,
                          const char *const *f_keys, const char *const *f_values);

    /* Declares a permission id, so commands can gate on it and role files
     * can grant it. */
    int (*register_permission)(const char *f_id, size_t f_id_length,
                               const char *f_display_name, size_t f_display_name_length,
                               const char *f_category, size_t f_category_length,
                               const char *f_owner_id, size_t f_owner_id_length);

    /* Reads a value from the owner's plugin config file
     * (config/plugins/<owner>.json), returning the fallback when absent. */
    const char *(*config_get)(const char *f_owner_id, size_t f_owner_id_length,
                              const char *f_key, size_t f_key_length,
                              const char *f_fallback, size_t f_fallback_length,
                              size_t *f_out_length);
} AkashiFfi;

#ifdef __cplusplus
}
#endif
