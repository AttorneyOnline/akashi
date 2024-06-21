#ifndef AKASHIDEFS_H
#define AKASHIDEFS_H

#include <QString>
#include <qnamespace.h>

namespace akashi {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
using SplitBehavior = QString::SplitBehavior;
#else
using SplitBehavior = Qt::SplitBehaviorFlags;
#endif
const SplitBehavior KeepEmptyParts = SplitBehavior::KeepEmptyParts;
const SplitBehavior SkipEmptyParts = SplitBehavior::SkipEmptyParts;
}

#endif // AKASHIDEFS_H
