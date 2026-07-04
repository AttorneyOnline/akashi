#include "core/arup_broadcaster.h"

#include "proto/packet.h"

namespace akashi {

ArupBroadcaster::ArupBroadcaster(QObject *parent) :
    QObject(parent)
{
    m_flush_timer.setSingleShot(true);
    m_flush_timer.setInterval(0);
    connect(&m_flush_timer, &QTimer::timeout, this, &ArupBroadcaster::flush);
}

void ArupBroadcaster::addArea(Area *area, int floorId)
{
    m_areas.append(area);
    if (floorId >= m_floor_areas.size())
        m_floor_areas.resize(floorId + 1);
    m_floor_areas[floorId].append(area);

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

void ArupBroadcaster::removeArea(Area *area)
{
    area->disconnect(this);
    m_areas.removeAll(area);
    for (QVector<Area *> &floorAreas : m_floor_areas) {
        floorAreas.removeAll(area);
    }
}

void ArupBroadcaster::removeFloor(int floorId)
{
    if (floorId >= 0 && floorId < m_floor_areas.size()) {
        m_floor_areas.removeAt(floorId);
    }
}

void ArupBroadcaster::clear()
{
    for (Area *area : std::as_const(m_areas)) {
        area->disconnect(this);
    }
    m_areas.clear();
    m_floor_areas.clear();
}

void ArupBroadcaster::setOwnerFormatter(OwnerFormatter formatter)
{
    m_format_owner = std::move(formatter);
}

void ArupBroadcaster::sendFullArup(int clientId, int floorId)
{
    Q_EMIT arupUnicast(buildFloorArup(Type::PlayerCount, floorId), clientId);
    Q_EMIT arupUnicast(buildFloorArup(Type::Status, floorId), clientId);
    Q_EMIT arupUnicast(buildFloorArup(Type::Cm, floorId), clientId);
    Q_EMIT arupUnicast(buildFloorArup(Type::Lock, floorId), clientId);
}

void ArupBroadcaster::broadcastNow(Type type)
{
    m_dirty[static_cast<int>(type)] = false;
    for (int i = 0; i < m_floor_areas.size(); ++i)
        Q_EMIT arupFloorBroadcast(buildFloorArup(type, i), i);
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
            Type l_type = static_cast<Type>(i);
            for (int f = 0; f < m_floor_areas.size(); ++f)
                Q_EMIT arupFloorBroadcast(buildFloorArup(l_type, f), f);
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

Packet ArupBroadcaster::buildFloorArup(Type type, int floorId) const
{
    QStringList fields;
    fields.append(QString::number(static_cast<int>(type)));

    const auto &l_areas = (floorId >= 0 && floorId < m_floor_areas.size()) ? m_floor_areas[floorId] : m_areas;
    for (const Area *area : l_areas) {
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

int ArupBroadcaster::floorCount() const
{
    return m_floor_areas.size();
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
