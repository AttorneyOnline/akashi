#include "include/advertiser.h"

Advertiser::Advertiser(QString p_ip, int p_port, int p_ws_port,
                       int p_local_port, QString p_name, QString p_description,
                       QObject *parent)
    : QObject(parent)
{
  ip = p_ip;
  port = p_port;
  ws_port = p_ws_port;
  local_port = p_local_port;
  name = p_name;
  description = p_description;
}

void Advertiser::contactMasterServer()
{
  socket = new QTcpSocket(this);
  connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
  connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
  connect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));

  socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
  socket->connectToHost(ip, port);
}

void Advertiser::readData()
{
  // The information coming back from the MS isn't very useful
  // However, it can be useful to see it when debugging
  // TODO: master network debug switch
  qDebug() << "From MS:" << socket->readAll();
}

void Advertiser::socketConnected()
{
  // TODO: fire a signal here, i18n
  qDebug("Connected to the master server");
  QString concat_ports;
  if (ws_port == -1)
    concat_ports = QString::number(local_port);
  else
    concat_ports = QString::number(local_port) + "&" + QString::number(ws_port);

  AOPacket ao_packet("SCC", {concat_ports, name, description,
                             "akashi v" + QApplication::applicationVersion()});
  QByteArray data = ao_packet.toUtf8();

  socket->write(data);
  // TODO: master network debug switch
  qDebug() << "To MS:" << data;
  socket->flush();
}

void Advertiser::socketDisconnected()
{
  // TODO: fire a signal here, i18n
  qDebug("Connection to master server lost");
}
