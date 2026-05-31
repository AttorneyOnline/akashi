#pragma once

#include "akashi_core_export.h"

#include <QString>

namespace akashi {

// One piece of evidence as the network protocol sees it. The LE list packs all three
// parts into one packet field, sub-divided by a literal &.
class AKASHI_CORE_EXPORT Evidence
{
  public:
    QString name;
    QString description;
    QString image;

    // The packed LE field: every part fully escaped, including & to <and>,
    // so a literal & inside a part cannot break the client's split.
    QString toLeField() const;

    // Reads a packed LE field back. The split happens on the raw text
    // before the escape codes are decoded, mirroring the client.
    static Evidence fromLeField(const QString &f_field);
};

} // namespace akashi

