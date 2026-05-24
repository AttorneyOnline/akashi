#ifndef AKASHI_LOG_WRITER_H
#define AKASHI_LOG_WRITER_H

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

#endif // AKASHI_LOG_WRITER_H
