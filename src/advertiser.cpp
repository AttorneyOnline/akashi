//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "include/advertiser.h"

Advertiser::Advertiser(QString p_ip, int p_port, int p_ws_port,
                       int p_local_port, QString p_name, QString p_description,
                       QObject* parent)
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
    connect(socket, &QTcpSocket::readyRead, this, &Advertiser::readData);
    connect(socket, &QTcpSocket::connected, this, &Advertiser::socketConnected);
    connect(socket, &QTcpSocket::disconnected, this, &Advertiser::socketDisconnected);

    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    socket->connectToHost(ip, port);
}

void Advertiser::readData()
{
    // The information coming back from the MS isn't very useful
    // However, it can be useful to see it when debugging
#ifdef NET_DEBUG
    qDebug() << "From MS:" << socket->readAll();
#endif
}

void Advertiser::socketConnected()
{
    qDebug("Connected to the master server");
    QString concat_ports;
    if (ws_port == -1)
        concat_ports = QString::number(local_port);
    else
        concat_ports =
            QString::number(local_port) + "&" + QString::number(ws_port);

    AOPacket ao_packet("SCC",
                       {concat_ports, name, description,
                        "akashi " + QCoreApplication::applicationVersion()});
    QByteArray data = ao_packet.toUtf8();

    socket->write(data);
#ifdef NET_DEBUG
    qDebug() << "To MS:" << data;
#endif
    socket->flush();
}

void Advertiser::socketDisconnected()
{
    qDebug("Connection to master server lost");
}

Advertiser::~Advertiser()
{
    socket->deleteLater();
}
