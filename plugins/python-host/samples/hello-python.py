"""akashi-plugin
{
    "id": "akashi.hello-python",
    "version": "1.1.0",
    "dependencies": ["akashi.python-host"]
}
"""

# A whole akashi plugin in one Python file: the header above is its
# manifest, and dropping the file into bin/plugins is the entire install.
import akashi

greetings_seen = 0
modcalls_seen = 0


def on_greeting(payload):
    global greetings_seen
    greetings_seen += 1


def on_modcall(payload):
    global modcalls_seen
    modcalls_seen += 1
    akashi.log(f"modcall from {payload.get('char_name', '?')}: {payload.get('reason', '')}")


# A custom event another script plugin publishes, and a core server event.
akashi.subscribe_event("script.greeting", on_greeting)
akashi.subscribe_event("modcall", on_modcall)


def hello(ctx, args):
    greeting = "Hello from Python!"
    if args:
        greeting = "Hello from Python, " + " ".join(args) + "!"
    akashi.reply(ctx, f"{greeting} (you are client {akashi.client_id(ctx)})")


def greetings_command(ctx, args):
    akashi.reply(ctx, f"greetings seen: {greetings_seen}")


def modcalls_command(ctx, args):
    akashi.reply(ctx, f"modcalls seen: {modcalls_seen}")


def config_command(ctx, args):
    akashi.reply(ctx, "greeting setting: " + akashi.config_get("greeting", "not configured"))


akashi.register_command("pyhello", "/pyhello", "Says hello from Python.", hello)
akashi.register_command("pygreetings", "/pygreetings", "Counts the script greetings seen.", greetings_command)
akashi.register_command("pymodcalls", "/pymodcalls", "Counts the modcalls seen.", modcalls_command)
akashi.register_command("pyconfig", "/pyconfig", "Reads a value from the plugin's config file.", config_command)
