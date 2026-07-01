--[[ akashi-plugin
{
    "id": "akashi.showcase-lua",
    "version": "1.0.0",
    "dependencies": ["akashi.lua-host"]
}
--]]

-- Everything a Lua plugin can do, in one file. The header above is the
-- manifest; dropping the file into bin/plugins is the entire install.

-- Declared permissions show up in role files, so owners can grant them to
-- moderator roles like any built-in permission.
akashi.register_permission("showcase.curse", "Sparkle Curse", "scripting")

-- Per-plugin configuration, read from config/plugins/akashi.showcase-lua.json.
-- The fallback applies when the file or key does not exist.
local greeting_template = akashi.config_get("greeting", "Welcome to AREA, NAME!")

-- A sanction-activated text filter: it runs for anyone whose sanction set
-- holds "sparkle", the way the built-in curses work.
akashi.register_text_filter("sparkle", 360, false, function(text)
    return text .. " *sparkles*"
end)

-- An always-active filter sees every IC message. Returning false DROPS the
-- message before anyone hears it; returning nothing passes it through.
akashi.register_text_filter("lua.courtesy", 900, true, function(text)
    if text == "the forbidden word" then
        return false
    end
end)

-- A rule action for the area rule system: owners attach it to floors and
-- areas with /addrule or areas.json, passing key=value arguments. A before
-- action returning a string blocks the event with that reason.
akashi.register_rule_action("lua.no_word", "before", function(info)
    local banned = info.args.word or "banana"
    if string.find(info.payload.message or "", banned, 1, true) then
        return "The word '" .. banned .. "' is banned in this area."
    end
end)

-- Core server events, delivered as key/value payloads after they happen.
local last_join = "nobody yet"
akashi.subscribe_event("player_joined_area", function(payload)
    last_join = payload.char_name .. " (area " .. payload.area_id .. ")"
end)

local last_track = "silence"
akashi.subscribe_event("music_changed", function(payload)
    last_track = payload.track_name
end)

-- A permission-gated command using the target verbs: resolve the argument,
-- inspect and toggle a sanction, and talk to the target directly.
akashi.register_command("sparkle", "/sparkle <id>", "Toggles the sparkle curse on a client.", function(ctx, args)
    if akashi.target_id(ctx, 1) < 0 then
        akashi.reply(ctx, "No client with that ID found.")
        return
    end
    if akashi.target_has_sanction(ctx, 1, "sparkle") then
        akashi.target_set_sanction(ctx, 1, "sparkle", false)
        akashi.reply(ctx, "The sparkle curse is lifted.")
    else
        akashi.target_set_sanction(ctx, 1, "sparkle", true)
        akashi.reply(ctx, "The sparkle curse is cast.")
        akashi.target_reply(ctx, 1, "Everything you say glitters now.")
    end
end, "showcase.curse", 1)

-- A free command replying to the whole area, and publishing a custom event
-- any plugin in any language can subscribe to.
akashi.register_command("luaroll", "/luaroll [sides]", "Rolls a die for the whole area to see.", function(ctx, args)
    local sides = math.max(2, tonumber(args[1]) or 6)
    local rolled = math.random(sides)
    akashi.reply_to_area(ctx, akashi.player_name(ctx) .. " rolled a " .. rolled .. " out of " .. sides .. ".")
    akashi.publish_event("showcase.roll", { roller = akashi.player_name(ctx), rolled = rolled, sides = sides })
end)

-- Reads back what the event subscriptions have gathered.
akashi.register_command("lualast", "/lualast", "Shows the last join and track this plugin saw.", function(ctx, args)
    akashi.reply(ctx, "Last join: " .. last_join .. "\nLast track: " .. last_track)
end)

-- The context accessors: who is asking, where they stand, what they may do.
akashi.register_command("luainfo", "/luainfo", "Shows what the plugin knows about you.", function(ctx, args)
    akashi.reply(ctx, "You are " .. akashi.player_name(ctx)
        .. " playing " .. akashi.character(ctx)
        .. " in " .. akashi.area_name(ctx) .. " (area " .. akashi.area_id(ctx) .. ")"
        .. "\nModerator: " .. tostring(akashi.is_authenticated(ctx))
        .. "\nCan kick: " .. tostring(akashi.can_perform(ctx, "kick")))
end)

-- The configured template, filled in with live context.
akashi.register_command("luagreet", "/luagreet", "Greets the area with the configured template.", function(ctx, args)
    local line = greeting_template:gsub("AREA", akashi.area_name(ctx)):gsub("NAME", akashi.player_name(ctx))
    akashi.reply_to_area(ctx, line)
end)

-- A task on the server console's menu, run from the terminal without a
-- game client. console_print reaches the operator who ran the task, even
-- one attached remotely.
akashi.register_console_action("Roll a die on the console", function()
    akashi.console_print("the console rolled a " .. math.random(6))
end)
