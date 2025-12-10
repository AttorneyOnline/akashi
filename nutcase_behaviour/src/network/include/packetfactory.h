#pragma once

#include "service.h"

class PacketFactory : public Service
{
    PacketFactory(QObject *parent) : Service{parent} {}
    ~PacketFactory() = default;
};
