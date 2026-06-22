--[[ akashi-plugin
{
    "id": "akashi.hello-lua",
    "version": "1.0.0"
}
--]]

-- A whole akashi plugin in one Lua file: the header above is its manifest,
-- and dropping the file into bin/plugins is the entire install.
akashi.log("hello-lua loaded")

akashi.register_command("luahello", "/luahello", "Says hello from Lua.", function(ctx, args)
    local greeting = "Hello from Lua!"
    if #args > 0 then
        greeting = "Hello from Lua, " .. table.concat(args, " ") .. "!"
    end
    akashi.reply(ctx, greeting .. " (you are client " .. akashi.client_id(ctx) .. ")")
end)
