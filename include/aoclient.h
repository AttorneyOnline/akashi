#ifndef AOCLIENT_H
#define AOCLIENT_H

#include <QTcpSocket>
#include <QHostAddress>
#include <QCryptographicHash>

class AOClient
{
public:
    AOClient(QHostAddress p_remote_ip);
    ~AOClient();

    QString getHwid();
    void setHwid(QString p_hwid);

    QString getIpid();

    QHostAddress remote_ip;

private:
    QString hwid;
    QString ipid;
};

#endif // AOCLIENT_H
