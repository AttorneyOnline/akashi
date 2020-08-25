#ifndef AOCLIENT_H
#define AOCLIENT_H

#include <QCryptographicHash>
#include <QHostAddress>
#include <QTcpSocket>

class AOClient {
public:
  AOClient(QHostAddress p_remote_ip);
  ~AOClient();

  QString getHwid();
  void setHwid(QString p_hwid);

  QString getIpid();

  QHostAddress remote_ip;
  QString password;
  bool joined;
  int current_area;
  QString current_char;

private:
  QString hwid;
  QString ipid;
};

#endif // AOCLIENT_H
