#pragma once

#include "akashi/area_rule.h"
#include "akashi_core_export.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace akashi {
namespace config {

// Song name to its category and duration, as loaded from music.json.
using MusicList = QMap<QString, QPair<QString, int>>;

struct MusicCatalog
{
    MusicList songs;
    QStringList ordered;
};

// One rule as declared in areas.json; the action is looked up in the
// RuleRegistry when the declaration is applied, not when it is parsed.
struct RuleDeclaration
{
    QString event;
    RulePhase phase = RulePhase::Before;
    QString action;
    QVariantMap args;
};

struct AreaRulesConfig
{
    QHash<QString, QVector<RuleDeclaration>> floor_rules;
    QHash<int, QVector<RuleDeclaration>> area_rules;
};

AKASHI_CORE_EXPORT QStringList loadTextFile(const QString &f_path);
AKASHI_CORE_EXPORT MusicCatalog loadMusicList(const QString &f_path);
AKASHI_CORE_EXPORT AreaRulesConfig loadAreaRules(const QString &f_path);
AKASHI_CORE_EXPORT QStringList loadIpRangeBans(const QString &f_path);
AKASHI_CORE_EXPORT QList<quint32> loadBannedAsns(const QString &f_path);

} // namespace config
} // namespace akashi
