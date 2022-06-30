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

    if (raw_packet.isEmpty())
        return nullptr;

    QStringList packet_contents = raw_packet.split("#");
    if (raw_packet.at(0) == '#') {
        // The header is encrypted with FantaCrypt
        // This should never happen with AO2 2.4.3 or newer
        qDebug() << "FantaCrypt packet received";
        header = "Unknown";
        packet_contents.append("Unknown");
        return nullptr;
    }
    else {
        header = packet_contents[0];
    }

    packet_contents.removeFirst(); // Remove header
    packet_contents.removeLast();  // Remove anything trailing after delimiter
    contents = packet_contents;

    return PacketFactory::createPacket(header, contents);
}