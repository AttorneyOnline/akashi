#include "core/mmdb_reader.h"

#include "akashi/logging_categories.h"

#include <QDebug>
#include <QFile>
#include <QHostAddress>

namespace akashi {

// The byte sequence that marks the start of the metadata section.
static const QByteArray &metadataMarker()
{
    static const QByteArray s_marker = QByteArray("\xab\xcd\xef", 3) + "MaxMind.com";
    return s_marker;
}

bool MmdbReader::open(const QString &f_path)
{
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly)) {
        return false;
    }
    m_data = l_file.readAll();
    m_cache.clear();

    const qsizetype l_marker = m_data.lastIndexOf(metadataMarker());
    if (l_marker == -1) {
        qCCritical(akashiDb) << f_path << "is not a MaxMind database file.";
        return false;
    }

    quint32 l_offset = l_marker + metadataMarker().size();
    const QVariantMap l_metadata = readValue(l_offset).toMap();
    m_node_count = l_metadata.value("node_count").toUInt();
    m_record_size = l_metadata.value("record_size").toInt();
    m_ip_version = l_metadata.value("ip_version").toInt();
    m_tree_size = quint64(m_node_count) * m_record_size * 2 / 8;

    if (m_node_count == 0 || (m_record_size != 24 && m_record_size != 28 && m_record_size != 32)) {
        qCCritical(akashiDb) << f_path << "has an unsupported record size or is empty.";
        return false;
    }
    return true;
}

bool MmdbReader::isOpen() const
{
    return m_node_count != 0;
}

quint32 MmdbReader::asnForAddress(const QHostAddress &f_address)
{
    if (!isOpen()) {
        return 0;
    }

    const QString l_key = f_address.toString();
    const auto l_cached = m_cache.constFind(l_key);
    if (l_cached != m_cache.constEnd()) {
        return l_cached.value();
    }

    // The address becomes the bit path through the search tree.
    quint8 l_bits[16] = {};
    int l_depth = m_ip_version == 4 ? 32 : 128;
    bool l_is_v4 = false;
    const quint32 l_v4 = f_address.toIPv4Address(&l_is_v4);
    if (l_is_v4) {
        // IPv4 lives under 96 leading zero bits inside an IPv6 tree.
        const int l_start = m_ip_version == 4 ? 0 : 12;
        l_bits[l_start] = l_v4 >> 24;
        l_bits[l_start + 1] = l_v4 >> 16;
        l_bits[l_start + 2] = l_v4 >> 8;
        l_bits[l_start + 3] = l_v4;
    }
    else if (m_ip_version == 4) {
        return 0;
    }
    else {
        const Q_IPV6ADDR l_v6 = f_address.toIPv6Address();
        memcpy(l_bits, l_v6.c, 16);
    }

    quint32 l_asn = 0;
    quint32 l_node = 0;
    for (int l_bit = 0; l_bit < l_depth; l_bit++) {
        const int l_side = l_bits[l_bit / 8] >> (7 - l_bit % 8) & 1;
        const quint32 l_record = record(l_node, l_side);
        if (l_record == m_node_count) {
            break;
        }
        if (l_record > m_node_count) {
            quint32 l_offset = m_tree_size + (l_record - m_node_count);
            l_asn = readValue(l_offset).toMap().value("autonomous_system_number").toUInt();
            break;
        }
        l_node = l_record;
    }

    m_cache.insert(l_key, l_asn);
    return l_asn;
}

quint32 MmdbReader::record(quint32 f_node, int f_side) const
{
    const uchar *l_data = reinterpret_cast<const uchar *>(m_data.constData());
    if (m_record_size == 24) {
        const uchar *l_node = l_data + f_node * 6 + f_side * 3;
        return l_node[0] << 16 | l_node[1] << 8 | l_node[2];
    }
    if (m_record_size == 28) {
        const uchar *l_node = l_data + f_node * 7;
        if (f_side == 0) {
            return (l_node[3] & 0xF0) << 20 | l_node[0] << 16 | l_node[1] << 8 | l_node[2];
        }
        return (l_node[3] & 0x0F) << 24 | l_node[4] << 16 | l_node[5] << 8 | l_node[6];
    }
    const uchar *l_node = l_data + f_node * 8 + f_side * 4;
    return quint32(l_node[0]) << 24 | l_node[1] << 16 | l_node[2] << 8 | l_node[3];
}

QVariant MmdbReader::readValue(quint32 &f_offset) const
{
    const uchar *l_data = reinterpret_cast<const uchar *>(m_data.constData());
    const quint8 l_control = l_data[f_offset++];
    int l_type = l_control >> 5;
    if (l_type == 0) {
        l_type = 7 + l_data[f_offset++];
    }

    // Pointers jump to shared entries inside the data section.
    if (l_type == 1) {
        const int l_pointer_size = (l_control >> 3) & 0x3;
        quint32 l_pointer = l_control & 0x7;
        for (int i = 0; i <= l_pointer_size; i++) {
            l_pointer = l_pointer << 8 | l_data[f_offset++];
        }
        if (l_pointer_size == 1) {
            l_pointer += 2048;
        }
        else if (l_pointer_size == 2) {
            l_pointer += 526336;
        }
        else if (l_pointer_size == 3) {
            l_pointer = l_pointer & 0xFFFFFFFF;
        }
        quint32 l_target = m_tree_size + 16 + l_pointer;
        return readValue(l_target);
    }

    quint32 l_size = l_control & 0x1F;
    if (l_size == 29) {
        l_size = 29 + l_data[f_offset++];
    }
    else if (l_size == 30) {
        l_size = 285 + (l_data[f_offset] << 8 | l_data[f_offset + 1]);
        f_offset += 2;
    }
    else if (l_size == 31) {
        l_size = 65821 + (l_data[f_offset] << 16 | l_data[f_offset + 1] << 8 | l_data[f_offset + 2]);
        f_offset += 3;
    }

    switch (l_type) {
    case 2: // text
    {
        const QString l_text = QString::fromUtf8(m_data.constData() + f_offset, l_size);
        f_offset += l_size;
        return l_text;
    }
    case 3:  // double
    case 15: // float
    {
        f_offset += l_size;
        return 0.0;
    }
    case 4: // raw bytes
    {
        const QByteArray l_bytes = m_data.mid(f_offset, l_size);
        f_offset += l_size;
        return l_bytes;
    }
    case 5: // unsigned numbers of different widths
    case 6:
    case 9:
    case 10:
    {
        quint64 l_number = 0;
        for (quint32 i = 0; i < l_size && i < 8; i++) {
            l_number = l_number << 8 | l_data[f_offset + i];
        }
        f_offset += l_size;
        return l_number;
    }
    case 8: // signed number
    {
        qint32 l_number = 0;
        for (quint32 i = 0; i < l_size; i++) {
            l_number = l_number << 8 | l_data[f_offset + i];
        }
        f_offset += l_size;
        return l_number;
    }
    case 7: // map
    {
        QVariantMap l_map;
        for (quint32 i = 0; i < l_size; i++) {
            const QString l_key = readValue(f_offset).toString();
            l_map.insert(l_key, readValue(f_offset));
        }
        return l_map;
    }
    case 11: // array
    {
        QVariantList l_list;
        for (quint32 i = 0; i < l_size; i++) {
            l_list.append(readValue(f_offset));
        }
        return l_list;
    }
    case 14: // boolean, the size is the value
        return l_size != 0;
    default: // containers and end markers carry no data
        return QVariant();
    }
}

} // namespace akashi
