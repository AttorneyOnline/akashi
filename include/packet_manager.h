#ifndef PACKET_MANAGER_H
#define PACKET_MANAGER_H

#include <QString>
#include <QStringList>

class PacketManager{
public:
    static QString buildPacket(QString header, QStringList contents);
};

#endif // PACKET_MANAGER_H
