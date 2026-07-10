// AI-generated: written by Claude.
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QUrl>

#include <optional>

class QHttpServer;
class QHttpServerRequest;
class QHttpServerResponse;
class QTcpServer;

namespace akashi {

// One captured HTTP request, verbatim as it arrived.
struct WebhookRequest
{
    QString method;
    QString path;
    QString query;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;

    // The value of a header, matched case-insensitively; empty when absent.
    QByteArray header(const QByteArray &f_name) const;
    // The body parsed as a JSON object.
    std::optional<QJsonObject> json() const;
};

// A local HTTP endpoint for tests that exercise webhook senders: start()
// it, point the code under test at url(), wait for the posts to arrive,
// then inspect what was sent. Answers 204 like Discord does, or any
// status a test configures to simulate a failing remote.
class WebhookReceiver : public QObject
{
    Q_OBJECT

  public:
    explicit WebhookReceiver(QObject *parent = nullptr);

    // Listens on a random localhost port; false when no port could be bound.
    bool start();
    quint16 port() const;
    // The endpoint under f_path, e.g. http://127.0.0.1:port/webhook.
    QUrl url(const QString &f_path = QStringLiteral("/webhook")) const;

    // Every later response carries this status.
    void setResponseStatus(int f_status);

    int requestCount() const;
    // Spins the event loop until f_count requests have arrived.
    bool waitForRequests(int f_count, int f_timeout_ms = 5000);
    const WebhookRequest &requestAt(int f_index) const;
    // Pops the oldest captured request.
    std::optional<WebhookRequest> takeRequest();
    void clear();

  Q_SIGNALS:
    void requestReceived();

  private:
    QHttpServerResponse captureRequest(const QHttpServerRequest &f_request);

    QHttpServer *m_server = nullptr;
    QTcpServer *m_socket = nullptr;
    QList<WebhookRequest> m_requests;
    int m_response_status = 204;
};

} // namespace akashi
