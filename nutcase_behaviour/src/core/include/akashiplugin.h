#pragma once
class ServiceRegistry;

#include <QObject>

#include "akashi_core_global.h"

class AKASHI_CORE_EXPORT AkashiPlugin : public QObject
{
    Q_OBJECT

  public:
    virtual ~AkashiPlugin() = default;
    virtual bool initialize(ServiceRegistry *f_registry) = 0;
    virtual void shutdown() = 0;
    virtual QString name() = 0;

  protected:
    enum INITSTATUS : bool
    {
        INITFAIL = false,
        INITSUCCESS = true
    };
    ServiceRegistry *m_registry;
};

#define AkashiPlugin_iid "akashi.plugin/1.0"
Q_DECLARE_INTERFACE(AkashiPlugin, "akashi.plugin/1.0")
