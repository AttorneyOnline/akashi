#pragma once

#include "akashi_network_global.h"

#include <QStringList>
#include <QVariantList>

class PacketInfo
{
};

class AKASHI_NETWORK_EXPORT Packet
{
  public:
    Packet() = default;
    ~Packet() = default;

    int size() const;
    std::optional<QVariant> dataAt(int f_index) const;

    virtual void decode(const QByteArray &data) = 0;
    virtual const QByteArray encode() = 0;

  protected:
    inline const static QMap<QString, QString> escape_codes{
        {"%", "<percent>"},
        {"#", "<num>"},
        {"$", "<dollar>"},
        {"&", "<and>"},

    };
    QVariantList m_contents;
};
