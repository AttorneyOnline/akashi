#include "packet.h"

Packet::Packet(const QString &f_header)
    : m_header(f_header)
{}

QString Packet::header() const
{
    return m_header;
}

void Packet::setHeader(const QString &f_h)
{
    m_header = f_h;
}

bool Packet::has(const QString &f_key) const
{
    return m_data.contains(f_key);
}
