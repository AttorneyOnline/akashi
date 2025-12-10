#include "packet.h"

int Packet::size() const
{
    return m_contents.size();
}

std::optional<QVariant> Packet::dataAt(int f_index) const
{
    if (f_index >= size()) {
        return std::nullopt;
    }
    return m_contents.at(f_index);
}
