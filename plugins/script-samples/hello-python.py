"""akashi-plugin
{
    "id": "akashi.hello-python",
    "version": "1.0.0",
    "dependencies": ["akashi.python-host"]
}
"""

# A whole akashi plugin in one Python file: the header above is its
# manifest, and dropping the file into bin/plugins is the entire install.
import akashi

akashi.log("hello-python loaded")


def hello(ctx, args):
    greeting = "Hello from Python!"
    if args:
        greeting = "Hello from Python, " + " ".join(args) + "!"
    akashi.reply(ctx, f"{greeting} (you are client {akashi.client_id(ctx)})")


akashi.register_command("pyhello", "/pyhello", "Says hello from Python.", hello)
