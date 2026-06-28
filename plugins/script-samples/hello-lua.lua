--[[ akashi-plugin
{
    "id": "akashi.hello-lua",
    "version": "1.1.0",
    "dependencies": ["akashi.lua-host"]
}
--]]

-- A whole akashi plugin in one Lua file: the header above is its manifest,
-- and dropping the file into bin/plugins is the entire install.

-- A declared permission, so role files can grant the uwu curse.
akashi.register_permission("hello.uwu", "Uwu Curse", "scripting")

-- A text filter, active for anyone whose sanction set holds "uwu".
akashi.register_text_filter("uwu", 350, false, function(text)
    return text:gsub("[lr]", "w"):gsub("[LR]", "W")
end)

akashi.register_command("luahello", "/luahello", "Says hello from Lua.", function(ctx, args)
    local greeting = "Hello from Lua!"
    if #args > 0 then
        greeting = "Hello from Lua, " .. table.concat(args, " ") .. "!"
    end
    akashi.reply(ctx, greeting .. " (you are client " .. akashi.client_id(ctx) .. ")")
    akashi.publish_event("script.greeting", { from = "hello-lua" })
end)

akashi.register_command("luawhoami", "/luawhoami", "Tells you who and where you are.", function(ctx, args)
    akashi.reply(ctx, "You are " .. akashi.player_name(ctx) .. " playing " .. akashi.character(ctx)
        .. " in " .. akashi.area_name(ctx) .. " (area " .. akashi.area_id(ctx) .. ")")
end)

akashi.register_command("uwu", "/uwu <id>", "Toggles the uwu curse on a client.", function(ctx, args)
    if akashi.target_id(ctx, 1) < 0 then
        akashi.reply(ctx, "No client with that ID found.")
        return
    end
    if akashi.target_has_sanction(ctx, 1, "uwu") then
        akashi.target_set_sanction(ctx, 1, "uwu", false)
        akashi.reply(ctx, "The uwu curse is disengaged.")
    else
        akashi.target_set_sanction(ctx, 1, "uwu", true)
        akashi.reply(ctx, "The uwu curse is engaged.")
        akashi.target_reply(ctx, 1, "You feew a stwange powew...")
    end
end, "hello.uwu", 1)
