#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <functional>
#include <variant>

#include "akashi_core_global.h"

using PacketEncoder = std::function<QByteArray(const QVariantMap &)>;
using PacketData = std::variant<QStringList, QJsonObject>;

class AKASHI_CORE_EXPORT Packet
{
public:
    virtual ~Packet() = default;

    const QString &header() const { return m_header; }
    void setHeader(const QString &f_header) { m_header = f_header; }

    QByteArray encode(PacketEncoder f_encoder) { return f_encoder(m_data); }

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
    // Helper for QStringList decoding with defaults
    template<class T>
    void setFromList(const QString &f_key,
                     const QStringList &f_list,
                     int f_index,
                     const T &f_default)
    {
        if (f_list.size() > f_index) {
            if constexpr (std::is_same_v<T, int>) {
                set(f_key, f_list[f_index].toInt());
            } else if constexpr (std::is_same_v<T, QString>) {
                set(f_key, f_list[f_index]);
            } else {
                set(f_key, QVariant(f_list[f_index]).value<T>());
            }
        } else {
            set(f_key, f_default);
        }
    }

    // Helper for QJsonObject decoding with defaults
    template<class T>
    void setFromJson(const QString &f_key, const QJsonObject &f_json, const T &f_default)
    {
        if (f_json.contains(f_key)) {
            if constexpr (std::is_same_v<T, int>) {
                set(f_key, f_json[f_key].toInt());
            } else if constexpr (std::is_same_v<T, QString>) {
                set(f_key, f_json[f_key].toString());
            } else {
                set(f_key, f_json[f_key].toVariant().value<T>());
            }
        } else {
            set(f_key, f_default);
        }
    }

    QVariantMap m_data;
    QString m_header;
};
