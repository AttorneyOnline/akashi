#include "akashi/network_service.h"

#include <QNetworkAccessManager>

namespace akashi {

NetworkService::NetworkService() :
    m_manager(std::make_unique<QNetworkAccessManager>())
{}

NetworkService::~NetworkService() = default;

QString NetworkService::serviceId() const
{
    return "akashi.network";
}

ServiceVersion NetworkService::serviceVersion() const
{
    return {1, 0, 0};
}

QNetworkAccessManager *NetworkService::networkManager() const
{
    return m_manager.get();
}

} // namespace akashi
