#include "include/server.h"

Server::Server(int p_port, int p_ws_port, QObject* parent)
    : QObject(parent)
{
  server = new QTcpServer(this);
  connect(server, SIGNAL(newConnection()), this, SLOT(clientConnected()));

  port = p_port;
  ws_port = p_ws_port;
}

void Server::start()
{
    // TODO: websockets lul
    // Maybe websockets should be handled by a separate intermediate part of the code?
    // The idea being that it is a websocket server, and all it does is create a
    // local connection to the raw tcp server.
    // The main issue with this is that it will cause problems with bans, ipids, etc
    // But perhaps this can be negotiated by sending some extra data over?
    // No idea. I'll wait for long to read this massive comment and DM me on discord
    if(!server->listen(QHostAddress::Any, port))
    {
        // TODO: signal server start failed
        qDebug() << "Server error:" << server->errorString();
    }
    else
    {
        // TODO: signal server start success
        qDebug() << "Server listening on" << port;
    }
}

void Server::clientConnected()
{
    QTcpSocket* client = server->nextPendingConnection();
    AOClient ao_client;
    clients.insert(client, ao_client);
    connect(client, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
    connect(client, SIGNAL(readyRead()), this, SLOT(clientData()));

    AOPacket decryptor("decryptor", {"34"});
    client->write(decryptor.toUtf8());

    qDebug() << client->peerAddress().toString() << "connected";
}

void Server::clientDisconnected()
{
    if(QTcpSocket* client = dynamic_cast<QTcpSocket*>(sender())){
        qDebug() << client->peerAddress() << "disconnected";
        clients.remove(client);
    }
}

void Server::clientData()
{
    if(QTcpSocket* client = dynamic_cast<QTcpSocket*>(sender())){
        QString data = QString::fromUtf8(client->readAll());
        qDebug() << "From" << client->peerAddress() << ":" << data;

        if(is_partial) {
            data = partial_packet + data;
        }
        if(!data.endsWith("%")){
            is_partial = true;
        }

        AOPacket packet(data);
    }
}
