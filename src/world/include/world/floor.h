#pragma once

#include "akashi/area_rule.h"
#include "akashi/jukebox_policy.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace akashi {

// One floor (hub) of the map: a named group of areas with floor-wide rules
// of play. Floors are the y axis of the map grid; the areas on a floor sit
// at x positions in the order of area_ids. A server without a map layout
// has exactly one floor holding every area - today's flat world.
class Floor
{
  public:
    // Who may take a character within this floor. UniquePerArea is today's
    // behavior: taken characters are tracked per area. UniqueOnFloor makes
    // a character claimable once across ALL the floor's areas; Shared lets
    // any number of people play the same character. The floor-wide checks
    // need every area's state, so they run at the world level.
    enum class CharacterPolicy
    {
        UniquePerArea,
        UniqueOnFloor,
        Shared,
    };

    int id = 0;
    QString name;
    CharacterPolicy character_policy = CharacterPolicy::UniquePerArea;
    QVector<int> area_ids; // the floor's areas, in x order

    // The floor's music catalog. Seeded from music.json at startup, then
    // floor config can add, remove or replace entries. Every area on this
    // floor inherits this list as its baseline; area customs layer on top.
    QStringList music_ordered;
    QMap<QString, JukeboxSong> music_songs;

    // CDN domains approved for URL-based custom songs.
    QStringList approved_cdns;

    // Rules applied to every area on this floor (unless the area overrides).
    QVector<BeforeRuleEntry> before_rules;
    QVector<AfterRuleEntry> after_rules;
};

} // namespace akashi

