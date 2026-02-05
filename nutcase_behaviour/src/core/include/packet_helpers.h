#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <type_traits>

namespace PacketHelpers
{

// Convert value from QStringList at index with fallback to default
template<class T>
T fromStringList(const QStringList &f_list, int f_index, const T &f_default)
{
    if (f_list.size() <= f_index)
    {
        return f_default;
    }

    if constexpr (std::is_same_v<T, int>)
    {
        return f_list[f_index].toInt();
    }
    else if constexpr (std::is_same_v<T, QString>)
    {
        return f_list[f_index];
    }
    else
    {
        return QVariant(f_list[f_index]).value<T>();
    }
}

// Convert value from QJsonObject with fallback to default
template<class T>
T fromJson(const QString &f_key, const QJsonObject &f_json, const T &f_default)
{
    if (!f_json.contains(f_key))
    {
        return f_default;
    }

    if constexpr (std::is_same_v<T, int>)
    {
        return f_json[f_key].toInt();
    }
    else if constexpr (std::is_same_v<T, QString>)
    {
        return f_json[f_key].toString();
    }
    else
    {
        return f_json[f_key].toVariant().value<T>();
    }
}

} // namespace PacketHelpers
