#include "include/aopacket.h"

AOPacket::AOPacket(QString p_header, QStringList p_contents)
{
  header = p_header;
  contents = p_contents;
}

AOPacket::AOPacket(QString p_packet)
{
  QStringList packet_contents = p_packet.split("#");
  if (p_packet.at(0) == '#') {
    // The header is encrypted with FantaCrypt
    // The server always uses the same key for FantaCrypt
    // That way, we can just hardcode FantaCrypted headers
    // TODO: replace this with a key/value map?
    packet_contents.removeFirst();
    if (packet_contents[0] == "48E0")
      header = "HI";
    else if (packet_contents[0] == "493F")
      header = "ID";
    else if (packet_contents[0] == "615810BC07D12A5A")
      header = "askchaa";
    else
      header = packet_contents[0]; // If no known decryption exists, just leave
                                   // the packet as-is
  }
  else {
    header = packet_contents[0];
  }
  packet_contents.removeFirst(); // Remove header
  packet_contents.removeLast();  // Remove anything trailing after delimiter
  contents = packet_contents;
}

QString AOPacket::toString()
{
  QString ao_packet = header;
  for (int i = 0; i < contents.length(); i++) {
    ao_packet += "#" + contents[i];
  }
  ao_packet += "#%";

  return ao_packet;
}

QByteArray AOPacket::toUtf8()
{
  QString packet_string = toString();
  return packet_string.toUtf8();
}
