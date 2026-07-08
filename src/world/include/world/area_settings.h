#pragma once

#include <QString>

namespace akashi {

// The per-area settings a server owner tunes: seeded from the areas config
// file when the area is built, toggled by commands while the server runs.
// Live play state - documents, music, testimony, judge bars - is not a
// setting and lives elsewhere.
struct AreaSettings
{
    QString background = QStringLiteral("gs4");
    bool protected_area = false;
    bool iniswap_allowed = true;
    bool background_locked = false;
    bool blankposting_allowed = true;
    QString area_message;
    bool send_area_message_on_join = false;
    bool force_immediate = false;
    bool music_allowed = true;
    bool shownames_allowed = true;
    bool ignore_background_list = false;
    bool jukebox_enabled = false;
    bool play_command_enabled = false;
    bool wtce_allowed = true;
    bool shouts_allowed = true;
    bool medieval_mode = false;
};

} // namespace akashi
