#ifndef SERVER_H
#define SERVER_H

#include "include/aoclient.h"
#include "include/aopacket.h"
#include "include/area_data.h"
#include "include/ws_proxy.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QSettings>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

class AOClient;

class Server : public QObject {
    Q_OBJECT

  public:
    Server(int p_port, int p_ws_port, QObject* parent = nullptr);
    void start();
    AOClient* getClient(QString ipid);
    void updateCharsTaken(AreaData* area);
    void broadcast(AOPacket packet);

    int player_count;
    QStringList characters;
    QVector<AreaData*> areas;

  signals:

  public slots:
    void clientConnected();

  private:
    WSProxy* proxy;
    QTcpServer* server;

    int port;
    int ws_port;

    QVector<AOClient*> clients;
};

#endif // SERVER_H
