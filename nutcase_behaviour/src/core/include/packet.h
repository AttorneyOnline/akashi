#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <functional>

#include "akashi_core_global.h"

using PacketEncoder = std::function<QByteArray(const QVariantMap &)>;

class AKASHI_CORE_EXPORT Packet
{
public:
    virtual ~Packet() = default;

    const QString &header() const { return m_header; }
    void setHeader(const QString &f_header) { m_header = f_header; }

    QByteArray encode(PacketEncoder f_encoder) const { return f_encoder(m_data); }

    template<class T>
    T get(const QString &f_identifier) const
    {
        return m_data.value(f_identifier).value<T>();
    }

    template<class T>
    void set(const QString &f_identifier, const T &f_data)
    {
        m_data[f_identifier] = QVariant::fromValue(f_data);
    }

    bool contains(const QString &f_identifier) const { return m_data.contains(f_identifier); }

protected:
    QVariantMap m_data;
    QString m_header;
};
