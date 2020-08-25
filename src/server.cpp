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
    QTcpSocket* client = server->nextPendingConnection();
    AOClient* ao_client = new AOClient(client->peerAddress());
    clients.insert(client, ao_client);
    connect(client, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
    connect(client, SIGNAL(readyRead()), this, SLOT(clientData()));

    AOPacket decryptor(
        "decryptor", {"NOENCRYPT"}); // This is the infamous workaround for
                                     // tsuserver4 It should disable fantacrypt
                                     // completely in any client 2.4.3 or newer
    client->write(decryptor.toUtf8());

    qDebug() << client->peerAddress().toString() << "connected";
}

void Server::clientDisconnected()
{
    if (QTcpSocket* client = dynamic_cast<QTcpSocket*>(sender())) {
        qDebug() << client->peerAddress() << "disconnected";
        AOClient* ao_client = clients.value(client);
        if (ao_client->joined)
            player_count--;
        areas.value(ao_client->current_area)
            ->characters_taken[ao_client->current_char] = false;

        delete ao_client;
        clients.remove(client);
    }
}

void Server::clientData()
{
    if (QTcpSocket* client = dynamic_cast<QTcpSocket*>(sender())) {
        QString data = QString::fromUtf8(client->readAll());
        // qDebug() << "From" << client->peerAddress() << ":" << data;

        if (is_partial) {
            data = partial_packet + data;
        }
        if (!data.endsWith("%")) {
            is_partial = true;
        }

        QStringList all_packets = data.split("%");
        all_packets.removeLast(); // Remove the entry after the last delimiter

        for (QString single_packet : all_packets) {
            AOPacket packet(single_packet);
            handlePacket(packet, client);
        }
    }
}

void Server::handlePacket(AOPacket packet, QTcpSocket* socket)
{
    // TODO: like everything here should send a signal
    qDebug() << "Received packet:" << packet.header << ":" << packet.contents;
    AOClient* client = clients.value(socket);
    AreaData* area = areas.value(client->current_area);
    // Lord forgive me
    if (packet.header == "HI") {
        AOClient* client = clients.value(socket);
        client->setHwid(packet.contents[0]);

        AOPacket response(
            "ID", {"271828", "akashi", QApplication::applicationVersion()});
        socket->write(response.toUtf8());
    }
    else if (packet.header == "ID") {
        QSettings config("config.ini", QSettings::IniFormat);
        config.beginGroup("Options");
        QString max_players = config.value("max_players").toString();
        config.endGroup();

        // Full feature list as of AO 2.8.5
        // The only ones that are critical to ensuring the server works are
        // "noencryption" and "fastloading"
        // TODO: make the rest of these user configurable
        QStringList feature_list = {
            "noencryption", "yellowtext",       "prezoom",
            "flipping",     "customobjections", "fastloading",
            "deskmod",      "evidence",         "cccc_ic_support",
            "arup",         "casing_alserts",   "modcall_reason",
            "looping_sfx",  "additive",         "effects"};

        AOPacket response_pn("PN",
                             {QString::number(player_count), max_players});
        AOPacket response_fl("FL", feature_list);
        socket->write(response_pn.toUtf8());
        socket->write(response_fl.toUtf8());
    }
    else if (packet.header == "askchaa") {
        // TODO: add user configurable content
        // For testing purposes, we will just send enough to get things working
        AOPacket response("SI",
                          {QString::number(characters.length()), "0", "1"});
        qDebug() << response.toString();
        socket->write(response.toUtf8());
    }
    else if (packet.header == "RC") {
        AOPacket response("SC", characters);
        socket->write(response.toUtf8());
    }
    else if (packet.header == "RM") {
        AOPacket response("SM", {"~stop.mp3"});
        socket->write(response.toUtf8());
    }
    else if (packet.header == "RD") {
        player_count++;
        client->joined = true;

        QStringList chars_taken;
        for (QString cur_char : area->characters_taken.keys()) {
            chars_taken.append(area->characters_taken.value(cur_char)
                                   ? QStringLiteral("-1")
                                   : QStringLiteral("0"));
        }

        AOPacket response_cc("CharsCheck", chars_taken);
        AOPacket response_op("OPPASS", {"DEADBEEF"});
        AOPacket response_done("DONE", {});
        socket->write(response_cc.toUtf8());
        socket->write(response_op.toUtf8());
        socket->write(response_done.toUtf8());
    }
    else if (packet.header == "PW") {
        client->password = packet.contents[0];
    }
    else if (packet.header == "CC") {
        bool argument_ok;
        int char_id = packet.contents[1].toInt(&argument_ok);
        if (!argument_ok)
            return;

        QString char_selected = characters[char_id];
        bool taken = area->characters_taken.value(char_selected);
        if (taken || char_selected == "")
            return;

        if (client->current_char != "") {
            area->characters_taken[client->current_char] = false;
        }

        area->characters_taken[char_selected] = true;
        client->current_char = char_selected;

        QStringList chars_taken;
        for (QString cur_char : area->characters_taken.keys()) {
            chars_taken.append(area->characters_taken.value(cur_char)
                                   ? QStringLiteral("-1")
                                   : QStringLiteral("0"));
        }

        AOPacket response_cc("CharsCheck", chars_taken);
        AOPacket response_pv("PV", {"271828", "CID", packet.contents[1]});
        socket->write(response_pv.toUtf8());
        socket->write(response_cc.toUtf8());
    }
    else if (packet.header == "MS") {
        // TODO: validate, validate, validate
        broadcast(packet);
    }
    else if (packet.header == "CT") {
        // TODO: commands
        // TODO: zalgo strip
        broadcast(packet);
    }
    else if (packet.header == "CH") {
        // Why does this packet exist
        AOPacket response("CHECK", {});
        socket->write(response.toUtf8());
    }
    else if (packet.header == "what") {
        AOPacket response(
            "CT", {"Made with love", "by scatterflower and windrammer"});
    }
    else {
        qDebug() << "Unimplemented packet:" << packet.header;
        qDebug() << packet.contents;
    }
    socket->flush();
}

void Server::broadcast(AOPacket packet)
{
    for (QTcpSocket* client : clients.keys()) {
        client->write(packet.toUtf8());
        client->flush();
    }
}

QTcpSocket* Server::getClient(QString ipid)
{
    for (QTcpSocket* client : clients.keys()) {
        if (clients.value(client)->getIpid() == ipid)
            return client;
    }
    return nullptr;
}
