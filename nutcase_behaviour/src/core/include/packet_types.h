#pragma once

#include <QJsonObject>
#include <QStringList>
#include <variant>

// Common type definitions shared between Packet and PacketRegistry
using PacketData = std::variant<QStringList, QJsonObject>;
