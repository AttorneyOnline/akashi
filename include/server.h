#ifndef SERVER_H
#define SERVER_H

#include "include/aoclient.h"
#include "include/aopacket.h"

#include <QApplication>
#include <QDebug>
#include <QMap>
#include <QSettings>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

class Server : public QObject {
  Q_OBJECT

public:
  Server(int p_port, int p_ws_port, QObject *parent = nullptr);
  void start();

signals:

public slots:
  void clientConnected();
  void clientDisconnected();
  void clientData();

private:
  void handlePacket(AOPacket packet, QTcpSocket *socket);
  QTcpSocket *getClient(QString ipid);
  void broadcast(AOPacket packet);

  QTcpServer *server;

  int port;
  int ws_port;

  QMap<QTcpSocket *, AOClient *> clients;
  QString partial_packet;
  bool is_partial;

  int player_count;
};

#endif // SERVER_H
