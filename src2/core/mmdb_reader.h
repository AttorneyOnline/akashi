#ifndef MMDB_READER_H
#define MMDB_READER_H

#include "akashi_core_export.h"

#include <QString>
#include <QStringList>
#include <QVariant>

namespace akashi {

// Reads a MaxMind database file, like the GeoLite2 ASN database.
class AKASHI_CORE_EXPORT MmdbReader
{
  public:
    bool open(const QString &f_path);

    // All networks announced by the given ASNs, as CIDR strings.
    QStringList networksForAsns(const QList<quint32> &f_asns) const;

  private:
    quint32 record(quint32 f_node, int f_side) const;
    QVariant readValue(quint32 &f_offset) const;

    QByteArray m_data;
    quint32 m_node_count = 0;
    int m_record_size = 0;
    int m_ip_version = 0;
    quint32 m_tree_size = 0;
};

} // namespace akashi

#endif // MMDB_READER_H
