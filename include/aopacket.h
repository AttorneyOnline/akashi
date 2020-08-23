#ifndef PACKET_MANAGER_H
#define PACKET_MANAGER_H

#include <QByteArray>
#include <QString>
#include <QStringList>

class AOPacket {
public:
  AOPacket(QString p_header, QStringList p_contents);
  QString toString();
  QByteArray toUtf8();

private:
  QString header;
  QStringList contents;
};

#endif // PACKET_MANAGER_H
