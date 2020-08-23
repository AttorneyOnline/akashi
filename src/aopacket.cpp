#include <include/aopacket.h>

AOPacket::AOPacket(QString p_header, QStringList p_contents)
{
  header = p_header;
  contents = p_contents;
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
