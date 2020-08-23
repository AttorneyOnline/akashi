#include <include/packet_manager.h>

QString PacketManager::buildPacket(QString header, QStringList contents)
{
    QString ao_packet = header;
    for(int i = 0; i < contents.length(); i++){
        ao_packet += "#" + contents[i];
    }
    ao_packet += "#%";

    return ao_packet;
}
