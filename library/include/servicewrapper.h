#pragma once

#include "service.h"
#include <QObject>
#include <QPointer>
#include <type_traits>

// This is a non-owning wrapper. Because I say so. - Salanto

template <class T>
    requires std::is_base_of_v<QObject, T>
class ServiceWrapper : public Service
{
  public:
    ServiceWrapper() = delete;
    ServiceWrapper(T *f_ptr, const QString &f_identifier,
                   const QString &f_version, const QString &f_author,
                   QObject *parent = nullptr) : Service(parent),
                                                m_ptr(f_ptr)
    {
        m_service_properties["identifier"] = f_identifier;
        m_service_properties["version"] = f_version;
        m_service_properties["author"] = f_author;

        m_state = Service::OK;
    }

    T *get() { return m_ptr.data(); }
    T *operator->() { return m_ptr.data(); }
    bool isValid() const { return !m_ptr.isNull(); }

  private:
    QPointer<T> m_ptr;
};
