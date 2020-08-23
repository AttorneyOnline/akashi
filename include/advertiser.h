#ifndef MASTER_H
#define MASTER_H

#include <include/packet_manager.h>

#include <QString>
#include <QTcpSocket>
#include <QApplication>

class Advertiser : public QObject{
public:
    Advertiser(QString p_ip, int p_port, int p_ws_port, int p_local_port, QString p_name, QString p_description);
    void contactMasterServer();

private:
    QString ip;
    int port;
    int ws_port;
    int local_port;
    QString name;
    QString description;

    void readData();
};

#endif // MASTER_H
