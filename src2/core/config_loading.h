#pragma once

#include "akashi_core_export.h"
#include "typedefs.h"

#include <QList>
#include <QStringList>

namespace akashi {
namespace config {

struct MusicCatalog
{
    MusicList songs;
    QStringList ordered;
};

AKASHI_CORE_EXPORT QStringList loadTextFile(const QString &f_path);
AKASHI_CORE_EXPORT MusicCatalog loadMusicList(const QString &f_path);
AKASHI_CORE_EXPORT QStringList loadIpRangeBans(const QString &f_path);
AKASHI_CORE_EXPORT QList<quint32> loadBannedAsns(const QString &f_path);

} // namespace config
} // namespace akashi

