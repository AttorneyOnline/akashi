#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <memory>

class QNetworkAccessManager;

namespace akashi {

// Shares one QNetworkAccessManager so the server and plugins reuse connections.
// Use the manager only from the main thread, and handle each reply's own signals
// rather than the manager's finished signal, since the manager is shared.
class AKASHI_CORE_EXPORT NetworkService : public IService
{
  public:
    NetworkService();
    ~NetworkService() override;

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    QNetworkAccessManager *networkManager() const;

  private:
    std::unique_ptr<QNetworkAccessManager> m_manager;
};

} // namespace akashi

