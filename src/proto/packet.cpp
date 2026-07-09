#include "proto/packet.h"

#include "proto/ao2_protocol.h"

namespace akashi {

Packet::Packet(const QString &f_header, const QStringList &f_fields) :
    m_header(f_header),
    m_fields(f_fields)
{}

Packet Packet::parse(const QString &f_raw)
{
    if (f_raw.isEmpty() || f_raw.at(0) == '#' || f_raw.contains('%')) {
        return Packet();
    }

    QStringList l_parts = f_raw.split('#');
    const QString l_header = l_parts.takeFirst();
    if (l_header.isEmpty()) {
        return Packet();
    }

    // The last part is whatever trails behind the final separator.
    if (!l_parts.isEmpty()) {
        l_parts.removeLast();
    }
    for (QString &l_part : l_parts) {
        l_part = unescape(l_part);
    }
    return Packet(l_header, l_parts);
}

bool Packet::isNull() const
{
    return m_header.isEmpty();
}

QString Packet::header() const
{
    return m_header;
}

QStringList Packet::fields() const
{
    return m_fields;
}

QString Packet::field(int f_index) const
{
    return m_fields.value(f_index);
}

int Packet::fieldCount() const
{
    return m_fields.size();
}

void Packet::setField(int f_index, const QString &f_value)
{
    m_fields[f_index] = f_value;
}

void Packet::setHeader(const QString &f_header)
{
    m_header = f_header;
}

void Packet::setFields(const QStringList &f_fields)
{
    m_fields = f_fields;
}

void Packet::appendField(const QString &f_value)
{
    m_fields.append(f_value);
}

QString Packet::serialize() const
{
    // A zero-field packet is "HEADER#%", not "HEADER##%" - the doubled
    // delimiter reads as one phantom empty field to a strict parser. The
    // AO2 client's own serializer emits no field section when empty.
    if (m_fields.isEmpty()) {
        return m_header + "#%";
    }

    const bool l_is_evidence = m_header == QLatin1String(ao2::HEADER_LE);
    QStringList l_fields = m_fields;
    for (QString &l_field : l_fields) {
        l_field = escape(l_field, l_is_evidence);
    }
    return m_header + "#" + l_fields.join('#') + "#%";
}

QString Packet::escape(const QString &f_text, bool f_is_evidence)
{
    QString l_text = f_text;
    l_text.replace("#", "<num>").replace("%", "<percent>").replace("$", "<dollar>");
    if (!f_is_evidence) {
        l_text.replace("&", "<and>");
    }
    return l_text;
}

QString Packet::unescape(const QString &f_text)
{
    QString l_text = f_text;
    l_text.replace("<num>", "#").replace("<percent>", "%").replace("<dollar>", "$").replace("<and>", "&");
    return l_text;
}

} // namespace akashi
