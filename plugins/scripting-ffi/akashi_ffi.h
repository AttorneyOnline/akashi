/* AI-generated: written by Claude. */
/*
 * The flat C surface scripting hosts build on. This header is deliberately
 * self-contained: no Qt types, UTF-8 strings passed as pointer plus explicit
 * length, callbacks as plain function pointers with a userdata slot.
 *
 * ABI rules: within a minor release series the table only ever GROWS. New
 * functions are appended at the end, existing entries never change
 * signature or position, and abi_version counts up with every addition -
 * so a host built against an older header keeps working against every
 * PATCH release of the same minor series. A MINOR revision of akashi may
 * remove or change script-interface entries outright, with every removal
 * named in the release notes. Pin your host to a minor series and check
 * abi_version before using late entries.
 *
 * Threading: everything runs on the server's main thread. Returned string
 * pointers stay valid until the next FFI call, so hosts copy immediately.
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define AKASHI_FFI_ABI_VERSION 11

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

    /* Carries a before-rule's refusal back; valid during the callback. */
    typedef struct AkashiRuleResult AkashiRuleResult;

    /* A rule action, fired when an area event it is attached to happens. The
     * event's payload and the arguments the rule was attached with arrive as
     * key/value string pairs. f_player_state_id names the acting user slot -
     * a client may hold several, and a session's first slot reuses the
     * session id; the payload's client_session_id key names the session
     * behind it. f_result is non-null for a before-action; calling
     * rule_result_block on it refuses the event. After-actions get null. */
    typedef void (*AkashiRuleFn)(void *f_userdata,
                                 int f_player_state_id, int f_area_id, int f_floor_id,
                                 int f_payload_count, const char *const *f_payload_keys, const char *const *f_payload_values,
                                 int f_argument_count, const char *const *f_argument_keys, const char *const *f_argument_values,
                                 AkashiRuleResult *f_result);

    /* A task on the server console's menu. Prints through console_print. */
    typedef void (*AkashiConsoleFn)(void *f_userdata);

    /* A scheduled job's body, run on the main thread when the job fires. */
    typedef void (*AkashiJobFn)(void *f_userdata);

    /* Carries an interceptor's rewritten packet back; valid during the callback. */
    typedef struct AkashiPacketResult AkashiPacketResult;

    /* An outbound packet interceptor. The packet on its way to a client
     * arrives as its header plus f_field_count string fields. Return 1 to let
     * it through - rewritten if packet_result_set was called, unchanged
     * otherwise - or 0 to drop it for this recipient. */
    typedef int (*AkashiInterceptorFn)(void *f_userdata,
                                       const char *f_header, size_t f_header_length,
                                       int f_field_count, const char *const *f_fields, const size_t *f_field_lengths,
                                       AkashiPacketResult *f_result);

    /* One row from sql_query: f_count columns, names and values as parallel
     * arrays, valid only for the duration of the callback. */
    typedef void (*AkashiSqlRowFn)(void *f_userdata, int f_count,
                                   const char *const *f_columns, const char *const *f_values);

    /* A schema migration body, run inside sql_migrate's transaction: issue
     * the upgrade statements through sql_exec and return 1 on success, 0 to
     * roll the whole migration back. */
    typedef int (*AkashiMigrationFn)(void *f_userdata);

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

        /* Subscribes to a named event: any core catalog event by its id (the
         * wiki's Scripting page lists them with payloads; the older aliases ic_message,
         * ooc_message, player_joined_area and player_left_area still map to
         * their catalog ids), a placeless event (modcall, ban_issued,
         * kick_issued, player_disconnected, config_reloaded), or any custom
         * name published by a plugin. Handlers run after the event committed.
         * Payloads arrive with the context ids player_state_id,
         * client_session_id, area_id and floor_id injected; a key the event
         * itself carried wins over the injected value. Returns 1 on success. */
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

        /* --- Added in ABI version 3 --- */

        /* Registers a named rule action for the area rule system. Owners attach
         * it to floors and areas with /addrule, /floorrule or areas.json, with
         * key=value arguments the action receives on every fire. f_before picks
         * the phase: 1 gates the event, 0 reacts to it. */
        int (*register_rule_action)(const char *f_name, size_t f_name_length, int f_before,
                                    AkashiRuleFn f_action, void *f_userdata,
                                    const char *f_owner_id, size_t f_owner_id_length);

        /* Refuses the gated event from inside a before-action callback. */
        void (*rule_result_block)(AkashiRuleResult *f_result, const char *f_reason, size_t f_reason_length);

        /* --- Added in ABI version 4 --- */

        /* Puts a task on the server console's menu, so an operator runs it
         * from the terminal without a game client. */
        int (*register_console_action)(const char *f_title, size_t f_title_length,
                                       AkashiConsoleFn f_action, void *f_userdata,
                                       const char *f_owner_id, size_t f_owner_id_length);

        /* --- Added in ABI version 5 --- */

        /* Prints a line to the operator running the current console task -
         * whether they sit at the server's terminal or are attached remotely.
         * Outside a task callback the line goes to the server log. */
        void (*console_print)(const char *f_text, size_t f_text_length);

        /* --- Added in ABI version 6 --- */

        /* Every path below is confined to the plugin's own data folder
         * (data/plugins/<owner>/): a path escaping it is refused, so a plugin
         * can only touch its own files. SQL runs against the plugin's own
         * database (data/plugins/<owner>.db), isolated from every other. */

        /* Reads the owner's file, returning its contents, or empty when the
         * file is missing or the path escapes the folder. */
        const char *(*fs_read)(const char *f_owner_id, size_t f_owner_id_length,
                               const char *f_path, size_t f_path_length,
                               size_t *f_out_length);

        /* Writes f_data to the owner's file, atomically and space-checked,
         * creating parent folders. Returns 1 on success, 0 on a bad path or
         * a refused write. */
        int (*fs_write)(const char *f_owner_id, size_t f_owner_id_length,
                        const char *f_path, size_t f_path_length,
                        const char *f_data, size_t f_data_length);

        /* 1 when the owner's file exists inside its data folder, else 0. */
        int (*fs_exists)(const char *f_owner_id, size_t f_owner_id_length,
                         const char *f_path, size_t f_path_length);

        /* Writes a value into the owner's plugin config file
         * (config/plugins/<owner>.json). Returns 1 on success. */
        int (*config_set)(const char *f_owner_id, size_t f_owner_id_length,
                          const char *f_key, size_t f_key_length,
                          const char *f_value, size_t f_value_length);

        /* Runs a non-select statement on the owner's own database, binding
         * f_param_count '?' placeholders from f_params (each a UTF-8 string of
         * the matching f_param_lengths). Returns the affected row count, or -1
         * on error. */
        int (*sql_exec)(const char *f_owner_id, size_t f_owner_id_length,
                        const char *f_sql, size_t f_sql_length,
                        int f_param_count, const char *const *f_params, const size_t *f_param_lengths);

        /* Runs a select on the owner's database, binding f_param_count params,
         * and calls f_row for each row. Returns the row count, or -1 on error. */
        int (*sql_query)(const char *f_owner_id, size_t f_owner_id_length,
                         const char *f_sql, size_t f_sql_length,
                         int f_param_count, const char *const *f_params, const size_t *f_param_lengths,
                         AkashiSqlRowFn f_row, void *f_userdata);

        /* --- Added in ABI version 7 --- */

        /* Builds and posts a Discord message through the core webhook hook.
         * Each verb works on the owner's single in-progress draft: begin
         * starts a fresh one, the set/embed verbs shape it through the real
         * message builder, and discord_post sends it and clears the draft.
         * Without Discord support present, discord_post returns 0. */

        void (*discord_begin)(const char *f_owner_id, size_t f_owner_id_length);

        /* Sets a top-level field: "content", "username", "avatar_url" or "tts". */
        void (*discord_set)(const char *f_owner_id, size_t f_owner_id_length,
                            const char *f_key, size_t f_key_length,
                            const char *f_value, size_t f_value_length);

        /* Opens an embed; the embed verbs apply until discord_embed_end. */
        void (*discord_embed_begin)(const char *f_owner_id, size_t f_owner_id_length);

        /* Sets a simple embed field: "title", "description", "url", "color",
         * "timestamp", "image" or "thumbnail". */
        void (*discord_embed_set)(const char *f_owner_id, size_t f_owner_id_length,
                                  const char *f_key, size_t f_key_length,
                                  const char *f_value, size_t f_value_length);

        /* Sets the embed footer / author; icon_url and url may be empty. */
        void (*discord_embed_footer)(const char *f_owner_id, size_t f_owner_id_length,
                                     const char *f_text, size_t f_text_length,
                                     const char *f_icon_url, size_t f_icon_url_length);
        void (*discord_embed_author)(const char *f_owner_id, size_t f_owner_id_length,
                                     const char *f_name, size_t f_name_length,
                                     const char *f_url, size_t f_url_length,
                                     const char *f_icon_url, size_t f_icon_url_length);

        /* Appends one name/value field to the current embed. */
        void (*discord_embed_field)(const char *f_owner_id, size_t f_owner_id_length,
                                    const char *f_name, size_t f_name_length,
                                    const char *f_value, size_t f_value_length,
                                    int f_inline);

        /* Closes the current embed. */
        void (*discord_embed_end)(const char *f_owner_id, size_t f_owner_id_length);

        /* Posts the owner's draft to f_url through the core hook and clears
         * it. Returns 1 when the hook accepted it, 0 without Discord support. */
        int (*discord_post)(const char *f_owner_id, size_t f_owner_id_length,
                            const char *f_url, size_t f_url_length);

        /* --- Added in ABI version 8 --- */

        /* Declares a typed, validated setting in the owner's config file, so
         * a value of the wrong type is reported at startup and config_get
         * returns f_default when the file omits the key. f_type is "string",
         * "int", "bool" or "double". Returns 1 when the file value is valid
         * or absent, 0 when it fails validation. */
        int (*config_declare)(const char *f_owner_id, size_t f_owner_id_length,
                              const char *f_key, size_t f_key_length,
                              const char *f_type, size_t f_type_length,
                              const char *f_default, size_t f_default_length,
                              const char *f_description, size_t f_description_length);

        /* Brings the owner's database up to f_to_version: when its stored
         * schema version is lower, runs f_migration inside a transaction and
         * bumps PRAGMA user_version on success; otherwise does nothing.
         * Returns 1 when the database is at or above f_to_version afterwards,
         * 0 when the migration failed. */
        int (*sql_migrate)(const char *f_owner_id, size_t f_owner_id_length,
                           int f_to_version, AkashiMigrationFn f_migration, void *f_userdata);

        /* Runs a select against an EXISTING database as a pure reader, never
         * a writer: f_source is empty or "main" for the server database, or a
         * plugin id for that plugin's database. Params bind like sql_query and
         * f_row receives each row. Writes are refused by the read-only
         * connection. Returns the row count, or -1 on error. */
        int (*sql_read)(const char *f_source, size_t f_source_length,
                        const char *f_sql, size_t f_sql_length,
                        int f_param_count, const char *const *f_params, const size_t *f_param_lengths,
                        AkashiSqlRowFn f_row, void *f_userdata);

        /* --- Added in ABI version 9 --- */

        /* Schedules a repeating job under f_job_id (unique within the owner):
         * f_day is "daily" or a weekday name, f_time is "HH:MM". f_action runs
         * on the main thread each time it fires. Replaces a job with the same
         * id, and leaves with the plugin. Returns 1 on success, 0 on a bad
         * day/time. */
        int (*schedule_repeating)(const char *f_owner_id, size_t f_owner_id_length,
                                  const char *f_job_id, size_t f_job_id_length,
                                  const char *f_day, size_t f_day_length,
                                  const char *f_time, size_t f_time_length,
                                  AkashiJobFn f_action, void *f_userdata);

        /* Schedules a one-shot job at a future moment named like a sanction
         * time: a duration "1d12h", a weekday, or "01.01.2028 18:00". A moment
         * already past fires on the next tick. Returns 1, or 0 when the time
         * is unreadable. */
        int (*schedule_once)(const char *f_owner_id, size_t f_owner_id_length,
                             const char *f_job_id, size_t f_job_id_length,
                             const char *f_when, size_t f_when_length,
                             AkashiJobFn f_action, void *f_userdata);

        /* Cancels the owner's job by id. */
        void (*schedule_cancel)(const char *f_owner_id, size_t f_owner_id_length,
                                const char *f_job_id, size_t f_job_id_length);

        /* The job's next run time as "yyyy-MM-dd hh:mm", or empty for an
         * unknown id. */
        const char *(*schedule_next_run)(const char *f_owner_id, size_t f_owner_id_length,
                                         const char *f_job_id, size_t f_job_id_length,
                                         size_t *f_out_length);

        /* --- Added in ABI version 10 --- */

        /* Reads any readable area property by id. The keys are the area's
         * declared properties - name, status, background, evidence_mod,
         * player_count, is_protected, lock_state, music_allowed and the other
         * allow flags - and grow with the area, not with this ABI. Empty for
         * an unknown area or key. */
        const char *(*area_get)(int f_area_id, const char *f_key, size_t f_key_length, size_t *f_out_length);

        /* Sets a writable area property; only the properties an area marks
         * settable (status, lock_state, the allow flags) can change, and the
         * change reaches clients where it is visible. Structural edits stay
         * native. Returns 1 on success, 0 for an unknown area, a read-only or
         * unknown key, or a bad value. */
        int (*area_set)(int f_area_id, const char *f_key, size_t f_key_length,
                        const char *f_value, size_t f_value_length);

        /* Reads a floor property by id; f_key is "name". Empty when unknown. */
        const char *(*floor_get)(int f_floor_id, const char *f_key, size_t f_key_length, size_t *f_out_length);

        int (*world_area_count)(void);
        int (*world_floor_count)(void);

        /* Registers an outbound packet interceptor. An empty f_header sees
         * every outbound packet; otherwise only that header. Lower f_order
         * runs earlier. The interceptor may rewrite or drop the packet on its
         * way to each client - see AkashiInterceptorFn. Owned like every
         * registration; removed with the plugin. Returns 1 on success. */
        int (*register_outbound_interceptor)(const char *f_header, size_t f_header_length,
                                             int f_order,
                                             AkashiInterceptorFn f_interceptor, void *f_userdata,
                                             const char *f_owner_id, size_t f_owner_id_length);

        /* Hands the rewritten packet back from inside an interceptor callback:
         * a new header and f_field_count replacement fields. */
        void (*packet_result_set)(AkashiPacketResult *f_result,
                                  const char *f_header, size_t f_header_length,
                                  int f_field_count, const char *const *f_fields, const size_t *f_field_lengths);

        /* --- Added in ABI version 11 --- */

        /* Places a standing server-scope grant of f_permission for one person
         * (f_audience "person", f_key their IPID) or one role (f_audience
         * "role", f_key the role id). The grant carries the owner as its
         * provenance and leaves with the plugin. Everyone-shaped audiences
         * and the super wildcard are refused - those are config and core
         * territory - as is an unregistered permission name. Returns 1 on
         * success. */
        int (*grant)(const char *f_permission, size_t f_permission_length,
                     const char *f_audience, size_t f_audience_length,
                     const char *f_key, size_t f_key_length,
                     const char *f_owner_id, size_t f_owner_id_length);

        /* Removes exactly the matching grant the owner placed. Returns 1 when
         * one was removed. */
        int (*revoke)(const char *f_permission, size_t f_permission_length,
                      const char *f_audience, size_t f_audience_length,
                      const char *f_key, size_t f_key_length,
                      const char *f_owner_id, size_t f_owner_id_length);
    } AkashiFfi;

#ifdef __cplusplus
}
#endif
