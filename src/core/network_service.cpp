#include "akashi/network_service.h"

#include "akashi/logging_categories.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslError>

namespace akashi {

// A stalled transfer must fail instead of hanging its consumer forever.
static const int TRANSFER_TIMEOUT = 1000 * 30;

// Logged, never ignored: the request still fails on a certificate problem,
// but the operator gets to see why.
static void logSslErrors(QNetworkReply *f_reply, const QList<QSslError> &f_errors)
{
    for (const QSslError &l_error : f_errors) {
        qCWarning(akashiNet) << "SSL error on" << f_reply->url().toString() << "-" << l_error.errorString();
    }
}

NetworkService::NetworkService() :
    m_manager(std::make_unique<QNetworkAccessManager>())
{
    m_manager->setTransferTimeout(TRANSFER_TIMEOUT);
    QObject::connect(m_manager.get(), &QNetworkAccessManager::sslErrors, m_manager.get(), &logSslErrors);
}

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
