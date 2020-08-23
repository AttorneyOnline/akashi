#ifndef MASTER_H
#define MASTER_H

#include <include/aopacket.h>

#include <QApplication>
#include <QHostAddress>
#include <QString>
#include <QTcpSocket>

class Advertiser : public QObject {
  Q_OBJECT

public:
  Advertiser(QString p_ip, int p_port, int p_ws_port, int p_local_port,
             QString p_name, QString p_description);
  void contactMasterServer();

signals:

public slots:
  void readData();
  void socketConnected();
  void socketDisconnected();

private:
  QString ip;
  int port;
  int ws_port;
  int local_port;
  QString name;
  QString description;

  QTcpSocket *socket;
};

#endif // MASTER_H
