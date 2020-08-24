#ifndef SERVER_H
#define SERVER_H

#include "include/aopacket.h"
#include "include/aoclient.h"

#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>

class Server : public QObject {
  Q_OBJECT

public:
  Server(int p_port, int p_ws_port, QObject* parent = nullptr);
  void start();

signals:

public slots:
  void clientConnected();
  void clientDisconnected();
  void clientData();

private:
  QTcpServer* server;

  int port;
  int ws_port;

  QMap<QTcpSocket*, AOClient> clients;
  QString partial_packet;
  bool is_partial;
};

#endif // SERVER_H
