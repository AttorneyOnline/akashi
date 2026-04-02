#ifndef PROTO_TEXT_UTILS_H
#define PROTO_TEXT_UTILS_H

#include "akashi_core_export.h"

#include <QString>

namespace akashi {

// Removes the combining characters used for zalgo text.
AKASHI_CORE_EXPORT QString stripZalgo(QString f_text);

} // namespace akashi

#endif // PROTO_TEXT_UTILS_H
