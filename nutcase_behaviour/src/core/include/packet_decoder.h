#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "akashi_core_global.h"
#include "packet.h"
#include "packet_helpers.h"

/**
 * @brief PacketDecoder handles deserialization of packet data from various formats
 *
 * This class provides a fluent interface for decoding packet data from QStringList
 * and QJsonObject sources, with default value support. All deserialization logic
 * is contained here, keeping Packet as a simple data container.
 *
 * Example usage:
 *   PacketDecoder(packet)
 *       .fromList(data)
 *       .field<int>("id", 0, -1)
 *       .field<QString>("name", 1, "default");
 */
class AKASHI_CORE_EXPORT PacketDecoder
{
public:
    explicit PacketDecoder(Packet &f_packet) : m_packet(f_packet) {}

    // Decode from QStringList
    PacketDecoder &fromList(const QStringList &f_list)
    {
        m_list = f_list;
        m_useList = true;
        return *this;
    }

    // Decode from QJsonObject
    PacketDecoder &fromJson(const QJsonObject &f_json)
    {
        m_json = f_json;
        m_useList = false;
        return *this;
    }

    // Add a field to decode (list mode - with index)
    template<class T>
    PacketDecoder &field(const QString &f_key, int f_index, const T &f_default)
    {
        if (m_useList)
        {
            // Decode from list using PacketHelpers
            T value = PacketHelpers::fromStringList(m_list, f_index, f_default);
            m_packet.set(f_key, value);
        }
        else
        {
            // JSON mode - ignore index, use key
            T value = PacketHelpers::fromJson(f_key, m_json, f_default);
            m_packet.set(f_key, value);
        }

        return *this;
    }

    // Simplified field method for JSON (no index needed)
    template<class T>
    PacketDecoder &field(const QString &f_key, const T &f_default)
    {
        if (!m_useList)
        {
            T value = PacketHelpers::fromJson(f_key, m_json, f_default);
            m_packet.set(f_key, value);
        }

        return *this;
    }

private:
    Packet &m_packet;
    QStringList m_list;
    QJsonObject m_json;
    bool m_useList = true;
};
