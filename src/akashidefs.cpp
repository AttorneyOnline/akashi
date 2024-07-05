#include "akashidefs.h"

namespace akashi {
QString get_protocol_version_string()
{
    return QString::number(PROTOCOL_MAJOR_VERSION) + "." + QString::number(PROTOCOL_MINOR_VERSION) + "." + QString::number(PROTOCOL_PATCH_VERSION);
}
}
