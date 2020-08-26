#ifndef AOCLIENT_H
#define AOCLIENT_H

#include "include/aopacket.h"
#include "include/server.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QTcpSocket>

class Server;

class AOClient : public QObject {
    Q_OBJECT
  public:
    AOClient(Server* p_server, QTcpSocket* p_socket, QObject* parent = nullptr);
    ~AOClient();

    QString getHwid();
    void setHwid(QString p_hwid);

    QString getIpid();

    QHostAddress remote_ip;
    QString password;
    bool joined;
    int current_area;
    QString current_char;

  public slots:
    void clientDisconnected();
    void clientData();
    void sendPacket(AOPacket packet);

  private:
    Server* server;
    QTcpSocket* socket;

    void handlePacket(AOPacket packet);

    QString partial_packet;
    bool is_partial;

    QString hwid;
    QString ipid;
};

#endif // AOCLIENT_H
