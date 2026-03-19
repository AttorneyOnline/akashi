#ifndef SERVER_CONFIG_ENTRIES_H
#define SERVER_CONFIG_ENTRIES_H

#include "akashi/config_entry.h"

#include <QList>

// The declared settings of config.json.
AKASHI_CORE_EXPORT QList<akashi::ConfigEntry> serverConfigEntries();

// The declared settings of discord.json.
AKASHI_CORE_EXPORT QList<akashi::ConfigEntry> discordConfigEntries();

#endif // SERVER_CONFIG_ENTRIES_H
