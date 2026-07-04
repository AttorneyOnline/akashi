#pragma once

#include "akashi_core_export.h"

namespace akashi {

// The dialect-free form of a packet. Concrete packets derive from this,
// for example an ICMessage. Codecs decode into it and handlers act on it.
class AKASHI_CORE_EXPORT Message
{
  public:
    virtual ~Message() = default;
};

} // namespace akashi

