"""akashi-plugin
{
    "id": "akashi.showcase-python",
    "version": "1.0.0",
    "dependencies": ["akashi.python-host"]
}
"""

# Everything a Python plugin can do, in one file. The docstring above is
# the manifest; dropping the file into bin/plugins is the entire install.
import akashi

# Declared permissions show up in role files, so owners can grant them to
# moderator roles like any built-in permission.
akashi.register_permission("showcase.shout", "Shout Curse", "scripting")

# Per-plugin configuration, read from config/plugins/akashi.showcase-python.json.
signature = akashi.config_get("signature", "the python showcase")

# A sanction-activated text filter: it runs for anyone whose sanction set
# holds "shout". Returning a string rewrites the message; None passes it
# through; False would drop it.
def shout(text):
    return text.upper()


akashi.register_text_filter("shout", 370, False, shout)

# Plugins keep ordinary state between calls; this one keeps a note per
# player name.
notes = {}
rolls_witnessed = 0
modcalls_witnessed = 0


# A custom event published by the LUA showcase plugin - script plugins
# compose across languages through the event system.
def on_roll(payload):
    global rolls_witnessed
    rolls_witnessed += 1
    akashi.log(f"showcase-python saw {payload.get('roller', '?')} roll {payload.get('rolled', '?')}")


# A core server event, the same feed the discord plugin uses.
def on_modcall(payload):
    global modcalls_witnessed
    modcalls_witnessed += 1


akashi.subscribe_event("showcase.roll", on_roll)
akashi.subscribe_event("modcall", on_modcall)

# A rule action for the area rule system: an after action reacts to events
# that actually happened in areas it is attached to.
ic_tallied = 0


def tally_rule(info):
    global ic_tallied
    ic_tallied += 1


akashi.register_rule_action("py.tally", "after", tally_rule)


def tally_command(ctx, args):
    akashi.reply(ctx, f"ic messages tallied: {ic_tallied}")


def note_command(ctx, args):
    notes[akashi.player_name(ctx)] = " ".join(args)
    akashi.reply(ctx, "Noted. Read it back with /notes.")


def notes_command(ctx, args):
    name = akashi.player_name(ctx)
    if name in notes:
        akashi.reply(ctx, f"Your note: {notes[name]}")
    else:
        akashi.reply(ctx, "You have no note. Leave one with /note <text>.")


# A permission-gated command using the target verbs.
def shout_command(ctx, args):
    if akashi.target_id(ctx, 0) < 0:
        akashi.reply(ctx, "No client with that ID found.")
        return
    if akashi.target_has_sanction(ctx, 0, "shout"):
        akashi.target_set_sanction(ctx, 0, "shout", False)
        akashi.reply(ctx, "The shout curse is lifted.")
    else:
        akashi.target_set_sanction(ctx, 0, "shout", True)
        akashi.reply(ctx, "The shout curse is cast.")
        akashi.target_reply(ctx, 0, "Your inside voice is gone.")


# Reads back what the event subscriptions have gathered.
def tallies_command(ctx, args):
    akashi.reply(ctx, f"rolls witnessed: {rolls_witnessed}\nmodcalls witnessed: {modcalls_witnessed}")


# The context accessors: who is asking, where they stand, what they may do.
def stats_command(ctx, args):
    akashi.reply(ctx, f"You are {akashi.player_name(ctx)}"
                      f" playing {akashi.character(ctx)}"
                      f" in {akashi.area_name(ctx)} (area {akashi.area_id(ctx)})"
                      f"\nModerator: {akashi.is_authenticated(ctx)}"
                      f"\nCan ban: {akashi.can_perform(ctx, 'ban')}"
                      f"\n- {signature}")


# A task on the server console's menu, run from the terminal without a
# game client. console_print reaches the operator who ran the task, even
# one attached remotely.
def console_tallies():
    akashi.console_print(f"tallies: rolls={rolls_witnessed} modcalls={modcalls_witnessed} ic={ic_tallied}")


akashi.register_console_action("Show showcase tallies", console_tallies)

akashi.register_command("note", "/note <text>", "Leaves yourself a note.", note_command, "", 1)
akashi.register_command("notes", "/notes", "Reads your note back.", notes_command)
akashi.register_command("shout", "/shout <id>", "Toggles the shout curse on a client.", shout_command, "showcase.shout", 1)
akashi.register_command("pyrolls", "/pyrolls", "Counts events this plugin witnessed.", tallies_command)
akashi.register_command("pytally", "/pytally", "Counts IC messages the py.tally rule saw.", tally_command)
akashi.register_command("pystats", "/pystats", "Shows what the plugin knows about you.", stats_command)
