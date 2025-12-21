#pragma once

#include <QObject>

#include "service.h"

class PacketRegistry : public Service
{
    Q_OBJECT

public:
    PacketRegistry(ServiceRegistry *f_registry, QObject *parent)
        : Service{f_registry, parent}
    {
        m_service_properties = {{"author", "Salanto"},
                                {"version", "1.0.0"},
                                {"identifier", "akashi.core.packetregistry"}};
    }
};
