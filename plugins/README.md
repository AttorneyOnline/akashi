# akashi plugins

Every directory here is a standalone plugin subproject: it builds in-tree
with the server, or out of tree against an installed akashi package via
`find_package(akashi CONFIG)`.

The authoring documentation lives in the wiki.

The bundled plugins are worked examples: `sql-logger` (a log writer),
`discord-integration` (event observers and webhooks), the
`lua-host`/`python-host` pair with their `samples/`, and
`samples/hello-world` (the smallest complete native plugin).
