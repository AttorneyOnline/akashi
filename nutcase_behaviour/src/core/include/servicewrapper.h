#pragma once

#include <QObject>
#include "service.h"
#include "serviceregistry.h"

class ServiceRegistry;

/**
 * @brief The ServiceWrapper class exposes non-Service objects to the ServiceRegistry, allowing Qt objects to be reused across different child-services.
 */
template<class T>
    requires std::is_base_of_v<QObject, T>
class ServiceWrapper : public Service
{
  public:
      explicit ServiceWrapper(const QString &f_author,
                              const QString &f_version,
                              const QString &f_identifier,
                              T *f_ptr,
                              ServiceRegistry *f_registry,
                              QObject *parent = nullptr)
          : Service{f_registry, parent}
          , m_ptr{f_ptr}
      {
          m_service_properties = {{"author", f_author},
                                  {"version", f_version},
                                  {"identifier", f_identifier}};

          m_registry->registerService(this);
      }

      T *instance() { return m_ptr; }

  private:
      T *m_ptr;
};
