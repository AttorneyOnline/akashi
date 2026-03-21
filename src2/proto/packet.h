#ifndef PROTO_PACKET_H
#define PROTO_PACKET_H

#include "akashi_core_export.h"

#include <QString>
#include <QStringList>

namespace akashi {

// One AO2 network packet as plain data. Copies are cheap and serializing never changes the packet.
class AKASHI_CORE_EXPORT Packet
{
  public:
    Packet() = default;
    explicit Packet(const QString &f_header, const QStringList &f_fields = {});

    // Reads a raw wire string like "HI#1234#". Unusable input gives a null packet.
    static Packet parse(const QString &f_raw);

    bool isNull() const;
    QString header() const;
    QStringList fields() const;
    QString field(int f_index) const;
    int fieldCount() const;
    void setField(int f_index, const QString &f_value);

    // The escaped wire form, ending with the packet terminator.
    QString serialize() const;

    // AO2 escape codes. Evidence packets keep & as their own field separator.
    static QString escape(const QString &f_text, bool f_is_evidence = false);
    static QString unescape(const QString &f_text);

  private:
    QString m_header;
    QStringList m_fields;
};

} // namespace akashi

#endif // PROTO_PACKET_H
