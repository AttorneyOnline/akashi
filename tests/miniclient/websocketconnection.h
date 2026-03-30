// AI-built: mirrored verbatim from the AO2-Client codebase by Claude.
// Taken from the AO2-Client codebase (src/network/websocketconnection.h).
// Only the AOApplication and ServerInfo dependencies were replaced with
// plain parameters; the framing and parsing code is unchanged.
#pragma once

#include "aopacket.h"

#include <QObject>
#include <QWebSocket>

class WebSocketConnection : public QObject
{
  Q_OBJECT

public:
  explicit WebSocketConnection(QObject *parent = nullptr);
  virtual ~WebSocketConnection();

  bool isConnected();

  void connectToServer(const QString &address, int port);
  void disconnectFromServer();

  void sendPacket(AOPacket packet);

Q_SIGNALS:
  void connectedToServer();
  void disconnectedFromServer();
  void errorOccurred(QString error);

  void receivedPacket(AOPacket packet);

private:
  QWebSocket *m_socket;
  QAbstractSocket::SocketState m_last_state;

private Q_SLOTS:
  void onError();
  void onStateChanged(QAbstractSocket::SocketState state);
  void onTextMessageReceived(QString message);
};
