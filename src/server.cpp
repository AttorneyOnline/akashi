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
#include "include/server.h"

Server::Server(int p_port, int p_ws_port, QObject* parent) : QObject(parent)
{
    server = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(clientConnected()));

    port = p_port;
    ws_port = p_ws_port;

    player_count = 0;
}

void Server::start()
{
    // TODO: websockets lul
    // Maybe websockets should be handled by a separate intermediate part of the
    // code? The idea being that it is a websocket server, and all it does is
    // create a local connection to the raw tcp server. The main issue with this
    // is that it will cause problems with bans, ipids, etc But perhaps this can
    // be negotiated by sending some extra data over? No idea. I'll wait for
    // long to read this massive comment and DM me on discord
    //
    // Upon thinking about this a bit more, I realized basically all of the
    // communication only happens via QTcpSocket* pointers.
    // If the Qt WebSocket server gives me QTcpSockets to work with,
    // then they can all go into the same object. I doubt this is the case,
    // though
    if (!server->listen(QHostAddress::Any, port)) {
        // TODO: signal server start failed
        qDebug() << "Server error:" << server->errorString();
    }
    else {
        // TODO: signal server start success
        qDebug() << "Server listening on" << port;
    }

    proxy = new WSProxy(port, ws_port, this);
    if(ws_port != -1)
        proxy->start();

    QFile char_list("characters.txt");
    char_list.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!char_list.atEnd()) {
        characters.append(char_list.readLine().trimmed());
    }

    // TODO: actually read areas from config
    areas.append(new AreaData(characters));
    areas[0]->name = "basement lol";
}

void Server::clientConnected()
{
    QTcpSocket* socket = server->nextPendingConnection();
    AOClient* client = new AOClient(this, socket, this);
    clients.append(client);
    connect(socket, &QTcpSocket::disconnected, client,
            &AOClient::clientDisconnected);
    connect(socket, &QTcpSocket::disconnected, this, [=] {
        qDebug() << "removed client" << client->getIpid();
        clients.removeAll(client);
        delete client;
    });
    connect(socket, &QTcpSocket::readyRead, client, &AOClient::clientData);

    AOPacket decryptor(
        "decryptor", {"NOENCRYPT"}); // This is the infamous workaround for
                                     // tsuserver4. It should disable fantacrypt
                                     // completely in any client 2.4.3 or newer
    client->sendPacket(decryptor);

    qDebug() << client->remote_ip.toString() << "connected";
}

void Server::updateCharsTaken(AreaData* area)
{
    QStringList chars_taken;
    for (QString cur_char : area->characters_taken.keys()) {
        chars_taken.append(area->characters_taken.value(cur_char)
                               ? QStringLiteral("-1")
                               : QStringLiteral("0"));
    }

    AOPacket response_cc("CharsCheck", chars_taken);
    broadcast(response_cc);
}

void Server::broadcast(AOPacket packet)
{
    // TODO: make this selective to the current area only
    for (AOClient* client : clients) {
        client->sendPacket(packet);
    }
}

AOClient* Server::getClient(QString ipid)
{
    for (AOClient* client : clients) {
        if (client->getIpid() == ipid)
            return client;
    }
    return nullptr;
}
