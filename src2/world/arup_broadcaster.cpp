#include "world/arup_broadcaster.h"

#include "proto/packet.h"

namespace akashi {

ArupBroadcaster::ArupBroadcaster(QObject *parent) :
    QObject(parent)
{
    m_flush_timer.setSingleShot(true);
    m_flush_timer.setInterval(0);
    connect(&m_flush_timer, &QTimer::timeout, this, &ArupBroadcaster::flush);
}

void ArupBroadcaster::addArea(Area *area)
{
    m_areas.append(area);

    connect(area, &Area::playerCountChanged, this, [this]() {
        markDirty(Type::PlayerCount);
    });
    connect(area, &Area::statusChanged, this, [this]() {
        markDirty(Type::Status);
    });
    connect(area, &Area::ownersChanged, this, [this]() {
        markDirty(Type::Cm);
    });
    connect(area, &Area::lockStateChanged, this, [this]() {
        markDirty(Type::Lock);
    });
}

void ArupBroadcaster::setOwnerFormatter(OwnerFormatter formatter)
{
    m_format_owner = std::move(formatter);
}

void ArupBroadcaster::sendFullArup(int clientId)
{
    Q_EMIT arupUnicast(buildArup(Type::PlayerCount), clientId);
    Q_EMIT arupUnicast(buildArup(Type::Status), clientId);
    Q_EMIT arupUnicast(buildArup(Type::Cm), clientId);
    Q_EMIT arupUnicast(buildArup(Type::Lock), clientId);
}

void ArupBroadcaster::broadcastNow(Type type)
{
    m_dirty[static_cast<int>(type)] = false;
    Q_EMIT arupBroadcast(buildArup(type));
}

void ArupBroadcaster::markDirty(Type type)
{
    m_dirty[static_cast<int>(type)] = true;
    if (!m_flush_timer.isActive()) {
        m_flush_timer.start();
    }
}

void ArupBroadcaster::flush()
{
    for (int i = 0; i < 4; ++i) {
        if (m_dirty[i]) {
            m_dirty[i] = false;
            Q_EMIT arupBroadcast(buildArup(static_cast<Type>(i)));
        }
    }
}

Packet ArupBroadcaster::buildArup(Type type) const
{
    QStringList fields;
    fields.append(QString::number(static_cast<int>(type)));

    for (const Area *area : m_areas) {
        switch (type) {
        case Type::PlayerCount:
            fields.append(QString::number(area->playerCount()));
            break;
        case Type::Status:
            fields.append(area->status());
            break;
        case Type::Cm:
            fields.append(formatOwners(area));
            break;
        case Type::Lock:
            fields.append(lockString(area->lockState()));
            break;
        }
    }

    return Packet(QStringLiteral("ARUP"), fields);
}

QString ArupBroadcaster::formatOwners(const Area *area) const
{
    if (area->owners().isEmpty()) {
        return QStringLiteral("FREE");
    }

    QStringList entries;
    for (int owner_id : area->owners()) {
        const QString entry = m_format_owner ? m_format_owner(owner_id) : QString();
        if (!entry.isEmpty()) {
            entries.append(entry);
        }
    }

    return entries.isEmpty() ? QStringLiteral("FREE") : entries.join(QStringLiteral(", "));
}

QString ArupBroadcaster::lockString(Area::LockState state)
{
    switch (state) {
    case Area::LockState::Locked:
        return QStringLiteral("LOCKED");
    case Area::LockState::Spectatable:
        return QStringLiteral("SPECTATABLE");
    default:
        return QStringLiteral("FREE");
    }
}

} // namespace akashi
