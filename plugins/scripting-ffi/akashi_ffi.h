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
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AKASHI_FFI_ABI_VERSION 1

/* Valid only for the duration of a command callback. */
typedef struct AkashiCommandContext AkashiCommandContext;

/* A command handler. f_argv holds f_argc UTF-8, NUL-terminated arguments. */
typedef void (*AkashiCommandFn)(void *f_userdata, AkashiCommandContext *f_context,
                                int f_argc, const char *const *f_argv);

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

    /* Removes every command the owner registered. */
    void (*unregister_owner)(const char *f_owner_id, size_t f_owner_id_length);

    /* Replies to the invoker of the running command. */
    void (*reply)(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length);

    /* Replies to everyone in the invoker's area. */
    void (*reply_to_area)(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length);

    /* The invoker's client id. */
    int (*client_id)(AkashiCommandContext *f_context);
} AkashiFfi;

#ifdef __cplusplus
}
#endif
