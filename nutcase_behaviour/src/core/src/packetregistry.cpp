#include "packetregistry.h"

QString PacketRegistry::makeKey(const QString &f_arch,
                                const QString &f_version,
                                const QString &f_header) const
{
    return f_arch + ":" + f_version + ":" + f_header;
}

std::optional<Packet *> PacketRegistry::create(const QString &f_arch,
                                               const QString &f_version,
                                               const QString &f_header,
                                               const QStringList &f_data)
{
    QString key = makeKey(f_arch, f_version, f_header);
    auto it = m_constructors.find(key);
    if (it != m_constructors.end()) {
        return (*it)(PacketData{f_data});
    }
    return std::nullopt;
}

std::optional<Packet *> PacketRegistry::create(const QString &f_arch,
                                               const QString &f_version,
                                               const QString &f_header,
                                               const QJsonObject &f_data)
{
    QString key = makeKey(f_arch, f_version, f_header);
    auto it = m_constructors.find(key);
    if (it != m_constructors.end()) {
        return (*it)(PacketData{f_data});
    }
    return std::nullopt;
}

void PacketRegistry::registerPacket(const QString &f_arch,
                                    const QString &f_version,
                                    const QString &f_header,
                                    PacketConstructor f_constructor)
{
    QString key = makeKey(f_arch, f_version, f_header);
    qDebug() << "Registering key" << key;
    m_constructors[key] = std::move(f_constructor);
}

void PacketRegistry::unregisterPacket(const QString &f_arch,
                                      const QString &f_version,
                                      const QString &f_header)
{
    QString key = makeKey(f_arch, f_version, f_header);
    m_constructors.remove(key);
}
