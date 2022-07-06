#include "include/packet/packet_factory.h"
#include "include/packet/packet_generic.h"

AOPacket *PacketFactory::createPacket(QString header, QStringList contents)
{
    if (!class_map.count(header)) {
        return createInstance<PacketGeneric>(header, contents);
    }

    return class_map[header](contents);
}

AOPacket *PacketFactory::createPacket(QString raw_packet)
{
    QString header;
    QStringList contents;
    
    if (raw_packet.at(0) == '#' || raw_packet.contains("%") || raw_packet.isEmpty()) {
        qDebug() << "FantaCrypt or otherwise invalid packet received";
        return PacketFactory::createPacket("Unknown", {"Unknown"});
    }

    QStringList packet_contents = raw_packet.split("#");
    header = packet_contents[0];

    packet_contents.removeFirst(); // Remove header
    packet_contents.removeLast();  // Remove anything trailing after delimiter
    contents = packet_contents;

    AOPacket *packet = PacketFactory::createPacket(header, contents);
    packet->unescapeContent();

    return packet;
}
