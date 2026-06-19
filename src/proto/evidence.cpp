#include "proto/evidence.h"

#include "proto/packet.h"

namespace akashi {

QString Evidence::toLeField() const
{
    return Packet::escape(name) + "&" + Packet::escape(description) + "&" + Packet::escape(image);
}

Evidence Evidence::fromLeField(const QString &f_field)
{
    const QStringList l_parts = f_field.split('&');
    Evidence l_evidence;
    l_evidence.name = Packet::unescape(l_parts.value(0));
    l_evidence.description = Packet::unescape(l_parts.value(1));
    l_evidence.image = Packet::unescape(l_parts.value(2));
    return l_evidence;
}

} // namespace akashi
