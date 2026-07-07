// AI-built: mirrored verbatim from the AO2-Client codebase by Claude.
// Taken from the AO2-Client codebase (src/network/websocketconnection.cpp).
// onTextMessageReceived is verbatim - it is the parsing the real client
// applies to everything the server sends.
#include "websocketconnection.h"

#include <QNetworkRequest>
#include <QUrl>

WebSocketConnection::WebSocketConnection(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_last_state(QAbstractSocket::UnconnectedState)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(m_socket, &QWebSocket::errorOccurred, this, &WebSocketConnection::onError);
#else
  connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &WebSocketConnection::onError);
#endif
  connect(m_socket, &QWebSocket::stateChanged, this, &WebSocketConnection::onStateChanged);
  connect(m_socket, &QWebSocket::textMessageReceived, this, &WebSocketConnection::onTextMessageReceived);
}

WebSocketConnection::~WebSocketConnection()
{
  m_socket->disconnect(this);
  disconnectFromServer();
}

bool WebSocketConnection::isConnected()
{
  return m_last_state == QAbstractSocket::ConnectedState;
}

void WebSocketConnection::connectToServer(const QString &address, int port)
{
  disconnectFromServer();

  QUrl url;
  url.setScheme("ws");
  url.setHost(address);
  url.setPort(port);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AttorneyOnline/2.11.0 (Desktop)"));

  m_socket->open(req);
}

void WebSocketConnection::disconnectFromServer()
{
  if (isConnected())
  {
    m_socket->close(QWebSocketProtocol::CloseCodeGoingAway);
  }
}

void WebSocketConnection::sendPacket(AOPacket packet)
{
  m_socket->sendTextMessage(packet.toString(true));
}

void WebSocketConnection::onError()
{
  Q_EMIT errorOccurred(m_socket->errorString());
}

void WebSocketConnection::onStateChanged(QAbstractSocket::SocketState state)
{
  m_last_state = state;
  switch (state)
  {
  default:
    break;

  case QAbstractSocket::ConnectedState:
    Q_EMIT connectedToServer();
    break;

  case QAbstractSocket::UnconnectedState:
    Q_EMIT disconnectedFromServer();
    break;
  }
}

void WebSocketConnection::onTextMessageReceived(QString message)
{
  if (!message.endsWith("#%"))
  {
    return;
  }
  message.chop(2);

  QStringList raw_content = message.split('#');
  const QString header = raw_content.takeFirst();
  for (QString &data : raw_content)
  {
    data = AOPacket::decode(data);
  }

  Q_EMIT receivedPacket(AOPacket(header, raw_content));
}
