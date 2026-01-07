#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>
#include <variant>

#include "akashi_core_global.h"
#include "service.h"

class Packet;

using PacketData = std::variant<QStringList, QJsonObject>;
using PacketConstructor = std::function<Packet *(const PacketData &)>;

class AKASHI_CORE_EXPORT PacketRegistry : public Service
{
    Q_OBJECT

public:
    PacketRegistry(ServiceRegistry *f_registry, QObject *parent);

    std::optional<Packet *> create(const QString &f_arch,
                                   const QString &f_version,
                                   const QString &f_header,
                                   const QStringList &f_data);

    std::optional<Packet *> create(const QString &f_arch,
                                   const QString &f_version,
                                   const QString &f_header,
                                   const QJsonObject &f_data);

    void registerPacket(const QString &f_arch,
                        const QString &f_version,
                        const QString &f_header,
                        PacketConstructor f_constructor);

    void unregisterPacket(const QString &f_arch, const QString &f_version, const QString &f_header);

private:
    QString makeKey(const QString &f_arch, const QString &f_version, const QString &f_header) const;

    QMap<QString, PacketConstructor> m_constructors;
};
