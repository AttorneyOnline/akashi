#pragma once

#include "akashi/log_event.h"

#include <QString>

namespace akashi {

class ILogWriter
{
  public:
    virtual ~ILogWriter() = default;
    virtual QString writerId() const = 0;
    virtual void write(const LogEvent &event) = 0;
    virtual void flush() {}
    virtual void maintenance() {}
};

} // namespace akashi
