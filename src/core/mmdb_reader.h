#pragma once

#include "akashi_core_export.h"

#include <QHash>
#include <QHostAddress>
#include <QString>
#include <QVariant>

namespace akashi {

// Reads a MaxMind database file, like the GeoLite2 ASN database.
class AKASHI_CORE_EXPORT MmdbReader
{
  public:
    bool open(const QString &f_path);
    bool isOpen() const;

    // The ASN that announces the given address, or 0 if it is unknown. Results are cached.
    quint32 asnForAddress(const QHostAddress &f_address);

  private:
    quint32 record(quint32 f_node, int f_side) const;
    QVariant readValue(quint32 &f_offset) const;

    QByteArray m_data;
    quint32 m_node_count = 0;
    int m_record_size = 0;
    int m_ip_version = 0;
    quint32 m_tree_size = 0;
    QHash<QString, quint32> m_cache;
};

} // namespace akashi
