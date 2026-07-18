// AI-generated: written by Claude.
#include "testtools/webhook_receiver.h"

#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QTcpServer>
#include <QtCore/qtestsupport_core.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QHttpHeaders>
#endif

namespace akashi {

// The wire verb of a parsed method, so tests compare plain strings.
static QString methodName(QHttpServerRequest::Method f_method)
{
    switch (f_method) {
    case QHttpServerRequest::Method::Get:
        return QStringLiteral("GET");
    case QHttpServerRequest::Method::Put:
        return QStringLiteral("PUT");
    case QHttpServerRequest::Method::Delete:
        return QStringLiteral("DELETE");
    case QHttpServerRequest::Method::Post:
        return QStringLiteral("POST");
    case QHttpServerRequest::Method::Head:
        return QStringLiteral("HEAD");
    case QHttpServerRequest::Method::Options:
        return QStringLiteral("OPTIONS");
    case QHttpServerRequest::Method::Patch:
        return QStringLiteral("PATCH");
    case QHttpServerRequest::Method::Connect:
        return QStringLiteral("CONNECT");
    case QHttpServerRequest::Method::Trace:
        return QStringLiteral("TRACE");
    default:
        return QStringLiteral("UNKNOWN");
    }
}

QByteArray WebhookRequest::header(const QByteArray &f_name) const
{
    for (const auto &[l_name, l_value] : headers) {
        if (l_name.compare(f_name, Qt::CaseInsensitive) == 0) {
            return l_value;
        }
    }
    return {};
}

std::optional<QJsonObject> WebhookRequest::json() const
{
    const QJsonDocument l_document = QJsonDocument::fromJson(body);
    if (!l_document.isObject()) {
        return std::nullopt;
    }
    return l_document.object();
}

WebhookReceiver::WebhookReceiver(QObject *parent) :
    QObject(parent)
{}

bool WebhookReceiver::start()
{
    if (m_socket) {
        return true;
    }
    m_socket = new QTcpServer(this);
    if (!m_socket->listen(QHostAddress::LocalHost, 0)) {
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    m_server = new QHttpServer(this);
    m_server->route(QStringLiteral("/"), [this](const QHttpServerRequest &f_request) {
        return captureRequest(f_request);
    });
    // The QUrl parameter swallows the whole remaining path, so any
    // webhook-shaped URL like /api/webhooks/id/token lands here.
    m_server->route(QStringLiteral("/<arg>"), [this](const QUrl &, const QHttpServerRequest &f_request) {
        return captureRequest(f_request);
    });
    m_server->bind(m_socket);
    return true;
}

quint16 WebhookReceiver::port() const
{
    return m_socket ? m_socket->serverPort() : 0;
}

QUrl WebhookReceiver::url(const QString &f_path) const
{
    const QString l_path = f_path.startsWith(QLatin1Char('/')) ? f_path : QLatin1Char('/') + f_path;
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(port()).arg(l_path));
}

void WebhookReceiver::setResponseStatus(int f_status)
{
    m_response_status = f_status;
}

void WebhookReceiver::setResponse(const QString &f_path, int f_status, const QByteArray &f_body,
                                  const QByteArray &f_content_type)
{
    const QString l_path = f_path.startsWith(QLatin1Char('/')) ? f_path : QLatin1Char('/') + f_path;
    m_responses.insert(l_path, {f_status, f_body, f_content_type});
}

int WebhookReceiver::requestCount() const
{
    return m_requests.size();
}

bool WebhookReceiver::waitForRequests(int f_count, std::chrono::milliseconds f_timeout)
{
    return QTest::qWaitFor([this, f_count] { return m_requests.size() >= f_count; }, int(f_timeout.count()));
}

const WebhookRequest &WebhookReceiver::requestAt(int f_index) const
{
    return m_requests.at(f_index);
}

std::optional<WebhookRequest> WebhookReceiver::takeRequest()
{
    if (m_requests.isEmpty()) {
        return std::nullopt;
    }
    return m_requests.takeFirst();
}

void WebhookReceiver::clear()
{
    m_requests.clear();
}

QHttpServerResponse WebhookReceiver::captureRequest(const QHttpServerRequest &f_request)
{
    WebhookRequest l_request;
    l_request.method = methodName(f_request.method());
    l_request.path = f_request.url().path();
    l_request.query = f_request.query().toString();
    l_request.body = f_request.body();
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const QHttpHeaders l_headers = f_request.headers();
    for (qsizetype i = 0; i < l_headers.size(); ++i) {
        const auto l_name = l_headers.nameAt(i);
        l_request.headers.append({QByteArray(l_name.data(), l_name.size()), l_headers.valueAt(i).toByteArray()});
    }
#else
    // Before 6.8 the headers already come as a list of name/value pairs.
    l_request.headers = f_request.headers();
#endif
    m_requests.append(l_request);
    Q_EMIT requestReceived();
    const auto l_canned = m_responses.constFind(l_request.path);
    if (l_canned != m_responses.constEnd()) {
        return QHttpServerResponse(l_canned->content_type, l_canned->body,
                                   static_cast<QHttpServerResponder::StatusCode>(l_canned->status));
    }
    return QHttpServerResponse(static_cast<QHttpServerResponder::StatusCode>(m_response_status));
}

} // namespace akashi
