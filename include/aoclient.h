#ifndef AOCLIENT_H
#define AOCLIENT_H

#include <QTcpSocket>

class AOClient
{
public:
    AOClient(QString p_remote_ip);

    QString hdid;
    QString remote_ip;

private:

};

#endif // AOCLIENT_H
