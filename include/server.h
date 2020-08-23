#ifndef SERVER_H
#define SERVER_H

#include <QString>
#include <QTcpServer>

class Server : public QObject {
  Q_OBJECT

public:
  Server(int p_port, int p_ws_port);
  void start();

private:
  int port;
  int ws_port;
};

#endif // SERVER_H
