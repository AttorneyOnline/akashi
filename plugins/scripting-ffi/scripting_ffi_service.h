#pragma once

#include "akashi/service.h"
#include "akashi_ffi.h"

// The service scripting hosts resolve to reach the C function table. The
// header is shared between the scripting-ffi plugin and its host plugins.
class ScriptingFfiService : public akashi::IService
{
  public:
    QString serviceId() const override { return QStringLiteral("akashi.scripting-ffi"); }
    akashi::ServiceVersion serviceVersion() const override { return {1, 0, 0}; }

    virtual const AkashiFfi *table() const = 0;
};
