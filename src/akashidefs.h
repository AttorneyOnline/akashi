#ifndef AKASHIDEFS_H
#define AKASHIDEFS_H

#include <QString>

namespace akashi {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
using SplitBehavior = QString::SplitBehavior;
#else
using SplitBehavior = Qt::SplitBehaviorFlags;
#endif
const SplitBehavior KeepEmptyParts = SplitBehavior::KeepEmptyParts;
const SplitBehavior SkipEmptyParts = SplitBehavior::SkipEmptyParts;

QString get_protocol_version_string();

const int PROTOCOL_MAJOR_VERSION = 1;
const int PROTOCOL_MINOR_VERSION = 0;
const int PROTOCOL_PATCH_VERSION = 0;
}
#endif // AKASHIDEFS_H
