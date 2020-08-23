#include "include/advertiser.h"

Advertiser::Advertiser(QString p_ip, int p_port, int p_ws_port, int p_local_port, QString p_name, QString p_description){
    ip = p_ip;
    port = p_port;
    ws_port = p_ws_port;
    local_port = p_local_port;
    name = p_name;
    description = p_description;
}

void Advertiser::contactMasterServer() {
    QString concat_ports;
    if(ws_port == -1)
        concat_ports = QString::number(local_port);
    else
        concat_ports = QString::number(local_port) + "&" + QString::number(ws_port);

    QString ao_packet = PacketManager::buildPacket("SCC", {concat_ports, name, description, "akashi v" + QApplication::applicationVersion()});
    QByteArray data = ao_packet.toUtf8();

    QTcpSocket socket(this);
    connect(&socket, SIGNAL(readyRead()), SLOT(readData()));

    socket.connectToHost(ip, port);
    if(socket.waitForConnected()){
        socket.write(data);
        qDebug() << "Advertisement sent to master server";
    }
}

void Advertiser::readData() {
    // The master server should never really send data back to us
    // But we handle it anyways, just in case this ever ends up being implemented
}
