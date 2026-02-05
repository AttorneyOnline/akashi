#pragma once

#include <QByteArray>
#include <QString>
#include <memory>

#include "akashi_core_global.h"
#include "packet.h"

/**
 * @brief PacketBuilder provides a fluent interface for constructing Packet objects
 *
 * Example usage:
 *   auto packet = PacketBuilder()
 *       .withHeader("MyPacket")
 *       .withField<int>("id", 123)
 *       .withField<QString>("name", "test")
 *       .build();
 */
class AKASHI_CORE_EXPORT PacketBuilder
{
public:
    PacketBuilder() : m_packet(std::make_unique<Packet>()) {}

    // Set the packet header
    PacketBuilder &withHeader(const QString &f_header)
    {
        m_packet->setHeader(f_header);
        return *this;
    }

    // Add a field to the packet
    template<class T>
    PacketBuilder &withField(const QString &f_key, const T &f_value)
    {
        m_packet->set(f_key, f_value);
        return *this;
    }

    // Add a field only if condition is true (conditional building)
    template<class T>
    PacketBuilder &withFieldIf(bool f_condition, const QString &f_key, const T &f_value)
    {
        if (f_condition)
        {
            m_packet->set(f_key, f_value);
        }
        return *this;
    }

    // Build and return the packet (transfers ownership)
    std::unique_ptr<Packet> build()
    {
        return std::move(m_packet);
    }

    // Build and return a raw pointer (caller takes ownership)
    Packet *buildRaw()
    {
        return m_packet.release();
    }

    // Build and encode in one step
    QByteArray buildAndEncode(PacketEncoder f_encoder)
    {
        return m_packet->encode(f_encoder);
    }

private:
    std::unique_ptr<Packet> m_packet;
};

/**
 * @brief TypedPacketBuilder provides compile-time type safety for specific packet types
 *
 * Example:
 *   auto myPacket = TypedPacketBuilder<MyCustomPacket>()
 *       .withHeader("Custom")
 *       .withField<int>("id", 42)
 *       .build();
 */
template<typename PacketType>
class TypedPacketBuilder
{
    static_assert(std::is_base_of_v<Packet, PacketType>,
                  "PacketType must derive from Packet");

public:
    TypedPacketBuilder() : m_packet(std::make_unique<PacketType>()) {}

    TypedPacketBuilder &withHeader(const QString &f_header)
    {
        m_packet->setHeader(f_header);
        return *this;
    }

    template<class T>
    TypedPacketBuilder &withField(const QString &f_key, const T &f_value)
    {
        m_packet->set(f_key, f_value);
        return *this;
    }

    template<class T>
    TypedPacketBuilder &withFieldIf(bool f_condition, const QString &f_key, const T &f_value)
    {
        if (f_condition)
        {
            m_packet->set(f_key, f_value);
        }
        return *this;
    }

    std::unique_ptr<PacketType> build()
    {
        return std::move(m_packet);
    }

    PacketType *buildRaw()
    {
        return m_packet.release();
    }

    QByteArray buildAndEncode(PacketEncoder f_encoder)
    {
        return m_packet->encode(f_encoder);
    }

private:
    std::unique_ptr<PacketType> m_packet;
};
