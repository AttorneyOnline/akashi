#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include "akashi_core_global.h"

class AKASHI_CORE_EXPORT Packet
{
private:
    QVariantMap m_data;
    QString m_header;

public:
    Packet(const QString &f_header = "");

    template<typename T>
        requires requires(T t) { QVariant::fromValue(t); } && (QMetaTypeId2<T>::Defined != 0)
    T get(const QString &f_key) const
    {
        qDebug() << "[FAST PATH] get:" << f_key;
        return m_data.contains(f_key) ? m_data[f_key].value<T>() : T{};
    }

    template<typename T>
        requires requires(T t) { QVariant::fromValue(t); } && (QMetaTypeId2<T>::Defined != 0)
    bool set(const QString &f_key, T f_data)
    {
        qDebug() << "[FAST PATH] set:" << f_key;
        m_data[f_key] = QVariant::fromValue(f_data);
        return true;
    }

    template<typename T>
        requires requires(T t) { QVariant::fromValue(t); } && (QMetaTypeId2<T>::Defined == 0)
    T get(const QString &f_key) const
    {
        qDebug() << "[SAFE PATH] get:" << f_key;
        if (!m_data.contains(f_key))
            return T{};

        const QVariant &l_var = m_data[f_key];
        if (!l_var.canConvert<T>()) {
            qWarning() << "Cannot convert key" << f_key << "to requested type";
            return T{};
        }
        return l_var.value<T>();
    }

    template<typename T>
        requires requires(T t) { QVariant::fromValue(t); } && (QMetaTypeId2<T>::Defined == 0)
    bool set(const QString &f_key, T f_data)
    {
        qDebug() << "[SAFE PATH] set:" << f_key;
        QVariant l_var = QVariant::fromValue(f_data);
        if (!l_var.isValid()) {
            qWarning() << "Failed to convert key" << f_key << "to QVariant";
            return false;
        }
        m_data[f_key] = l_var;
        return true;
    }

    QString header() const;
    void setHeader(const QString &f_h);
    bool has(const QString &f_key) const;
};
