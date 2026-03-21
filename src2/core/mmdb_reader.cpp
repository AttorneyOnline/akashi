#include "core/mmdb_reader.h"

#include <QDebug>
#include <QFile>
#include <QHostAddress>

namespace akashi {

// The byte sequence that marks the start of the metadata section.
static const QByteArray METADATA_MARKER = QByteArray("\xab\xcd\xef", 3) + "MaxMind.com";

bool MmdbReader::open(const QString &f_path)
{
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly)) {
        return false;
    }
    m_data = l_file.readAll();

    const qsizetype l_marker = m_data.lastIndexOf(METADATA_MARKER);
    if (l_marker == -1) {
        qCritical() << f_path << "is not a MaxMind database file.";
        return false;
    }

    quint32 l_offset = l_marker + METADATA_MARKER.size();
    const QVariantMap l_metadata = readValue(l_offset).toMap();
    m_node_count = l_metadata.value("node_count").toUInt();
    m_record_size = l_metadata.value("record_size").toInt();
    m_ip_version = l_metadata.value("ip_version").toInt();
    m_tree_size = quint64(m_node_count) * m_record_size * 2 / 8;

    if (m_node_count == 0 || (m_record_size != 24 && m_record_size != 28 && m_record_size != 32)) {
        qCritical() << f_path << "has an unsupported record size or is empty.";
        return false;
    }
    return true;
}

QStringList MmdbReader::networksForAsns(const QList<quint32> &f_asns) const
{
    struct Position
    {
        quint32 node;
        int depth;
        quint8 bits[16];
    };

    const int l_max_depth = m_ip_version == 4 ? 32 : 128;
    QStringList l_networks;
    QList<Position> l_stack;
    l_stack.append(Position{0, 0, {}});

    while (!l_stack.isEmpty()) {
        const Position l_position = l_stack.takeLast();
        for (int l_side = 0; l_side < 2; l_side++) {
            const quint32 l_record = record(l_position.node, l_side);
            if (l_record == m_node_count) {
                continue;
            }

            Position l_next = l_position;
            if (l_side == 1) {
                l_next.bits[l_position.depth / 8] |= 1 << (7 - l_position.depth % 8);
            }
            l_next.depth++;

            if (l_record < m_node_count) {
                if (l_next.depth < l_max_depth) {
                    l_next.node = l_record;
                    l_stack.append(l_next);
                }
                continue;
            }

            // The record points at a data entry, check if its ASN is wanted.
            quint32 l_offset = m_tree_size + (l_record - m_node_count);
            const QVariantMap l_entry = readValue(l_offset).toMap();
            const quint32 l_asn = l_entry.value("autonomous_system_number").toUInt();
            if (!f_asns.contains(l_asn)) {
                continue;
            }

            // IPv4 lives under 96 leading zero bits inside an IPv6 tree.
            bool l_is_mapped_v4 = m_ip_version == 6 && l_next.depth >= 96;
            for (int i = 0; l_is_mapped_v4 && i < 12; i++) {
                l_is_mapped_v4 = l_next.bits[i] == 0;
            }

            if (m_ip_version == 4 || l_is_mapped_v4) {
                const quint8 *l_v4 = m_ip_version == 4 ? l_next.bits : l_next.bits + 12;
                const quint32 l_address = l_v4[0] << 24 | l_v4[1] << 16 | l_v4[2] << 8 | l_v4[3];
                const int l_prefix = m_ip_version == 4 ? l_next.depth : l_next.depth - 96;
                l_networks.append(QHostAddress(l_address).toString() + "/" + QString::number(l_prefix));
            }
            else {
                Q_IPV6ADDR l_address;
                memcpy(l_address.c, l_next.bits, 16);
                l_networks.append(QHostAddress(l_address).toString() + "/" + QString::number(l_next.depth));
            }
        }
    }
    return l_networks;
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
