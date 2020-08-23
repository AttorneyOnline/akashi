#include "include/advertiser.h"

Advertiser::Advertiser(QString p_ip, int p_port, int p_ws_port, int p_local_port, QString p_name, QString p_description)
{
    ip = p_ip;
    port = p_port;
    ws_port = p_ws_port;
    local_port = p_local_port;
    name = p_name;
    description = p_description;
}

void Advertiser::contactMasterServer() {
    socket = new QTcpSocket(this);
    connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));

    socket->connectToHost(ip, port);

    if(socket->waitForConnected(1000)) {
        qDebug("Connected to master server");
    } else {
        qDebug() << "Master server socket error: " << socket->errorString();
    }
}

void Advertiser::readData() {
    // The master server should never really send data back to us
    // But we handle it anyways, just in case this ever ends up being implemented
}

void Advertiser::socketConnected() {
    QString concat_ports;
    if(ws_port == -1)
        concat_ports = QString::number(local_port);
    else
        concat_ports = QString::number(local_port) + "&" + QString::number(ws_port);

    QString ao_packet = PacketManager::buildPacket("SCC", {concat_ports, name, description, "akashi v" + QApplication::applicationVersion()});
    QByteArray data = ao_packet.toUtf8();

    socket->write(data);
    socket->flush();
    qDebug("Advertisement sent to master server");
}
