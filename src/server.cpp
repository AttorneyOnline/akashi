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
    timer = new QTimer();

    player_count = 0;

    db_manager = new DBManager();
}

void Server::start()
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    config.beginGroup("Options");
    QString bind_ip = config.value("ip", "all").toString();
    QHostAddress bind_addr;
    if (bind_ip == "all")
        bind_addr = QHostAddress::Any;
    else
        bind_addr = QHostAddress(bind_ip);
    if (bind_addr.protocol() != QAbstractSocket::IPv4Protocol && bind_addr.protocol() != QAbstractSocket::IPv6Protocol && bind_addr != QHostAddress::Any) {
        qDebug() << bind_ip << "is an invalid IP address to listen on! Server not starting, check your config.";
    }
    if (!server->listen(bind_addr, port)) {
        qDebug() << "Server error:" << server->errorString();
    }
    else {
        qDebug() << "Server listening on" << port;
    }

    MOTD = config.value("motd","MOTD is not set.").toString();

    proxy = new WSProxy(port, ws_port, this);
    if(ws_port != -1)
        proxy->start();

    QFile char_list("config/characters.txt");
    char_list.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!char_list.atEnd()) {
        characters.append(char_list.readLine().trimmed());
    }
    char_list.close();

    QFile music_file("config/music.txt");
    music_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!music_file.atEnd()) {
        music_list.append(music_file.readLine().trimmed());
    }
    music_file.close();
    if(music_list[0].contains(".")) // Add a default category if none exists
        music_list.insert(0, "==Music==");

    QFile bg_file("config/backgrounds.txt");
    bg_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!bg_file.atEnd()) {
        backgrounds.append(bg_file.readLine().trimmed());
    }
    bg_file.close();

    QSettings areas_ini("config/areas.ini", QSettings::IniFormat);
    area_names = areas_ini.childGroups(); // invisibly does a lexicographical sort, because Qt is great like that
    std::sort(area_names.begin(), area_names.end(), [] (const QString &a, const QString &b) {return a.split(":")[0].toInt() < b.split(":")[0].toInt();});
    QStringList sanitized_area_names;
    QStringList raw_area_names = area_names;
    for (QString area_name : area_names) {
        QStringList name_split = area_name.split(":");
        name_split.removeFirst();
        QString area_name_sanitized = name_split.join(":");
        sanitized_area_names.append(area_name_sanitized);
    }
    area_names = sanitized_area_names;
    for (int i = 0; i < raw_area_names.length(); i++) {
        QString area_name = raw_area_names[i];
        areas.insert(i, new AreaData(area_name, i));
    }
}

void Server::clientConnected()
{
    QTcpSocket* socket = server->nextPendingConnection();
    int user_id;
    QList<int> user_ids;
    for (AOClient* client : clients) {
        user_ids.append(client->id);
    }
    for (user_id = 0; user_id <= player_count; user_id++) {
        if (user_ids.contains(user_id))
            continue;
        else
            break;
    }
    AOClient* client = new AOClient(this, socket, this, user_id);
    if (db_manager->isIPBanned(socket->peerAddress())) {
        AOPacket ban_reason("BD", {db_manager->getBanReason(socket->peerAddress())});
        socket->write(ban_reason.toUtf8());
        socket->flush();
        client->deleteLater();
        socket->close();
        return;
    }
    clients.append(client);
    connect(socket, &QTcpSocket::disconnected, client,
            &AOClient::clientDisconnected);
    connect(socket, &QTcpSocket::disconnected, this, [=] {
        clients.removeAll(client);
        client->deleteLater();
    });
    connect(socket, &QTcpSocket::readyRead, client, &AOClient::clientData);

    AOPacket decryptor("decryptor", {"NOENCRYPT"}); // This is the infamous workaround for
                                                    // tsuserver4. It should disable fantacrypt
                                                    // completely in any client 2.4.3 or newer
    client->sendPacket(decryptor);
#ifdef NET_DEBUG
    qDebug() << client->remote_ip.toString() << "connected";
#endif
}

void Server::updateCharsTaken(AreaData* area)
{
    QStringList chars_taken;
    for (QString cur_char : characters) {
        chars_taken.append(area->characters_taken.contains(getCharID(cur_char))
                               ? QStringLiteral("-1")
                               : QStringLiteral("0"));
    }

    AOPacket response_cc("CharsCheck", chars_taken);
    broadcast(response_cc, area->index);
}

void Server::broadcast(AOPacket packet, int area_index)
{
    for (AOClient* client : clients) {
        if (client->current_area == area_index)
            client->sendPacket(packet);
    }
}

void Server::broadcast(AOPacket packet)
{
    for (AOClient* client : clients) {
        client->sendPacket(packet);
    }
}

QString Server::getServerName()
{
    QSettings settings("config/config.ini", QSettings::IniFormat);
    settings.beginGroup("Options");
    QString server_name = settings.value("server_name", "Akashi").toString();
    return server_name;
}

int Server::getDiceValue(QString value_type)
{
    QSettings settings("config/config.ini", QSettings::IniFormat);

    settings.beginGroup("Dice");
    int value = settings.value(value_type, "100").toUInt();
    settings.endGroup();
    return value;
}

AOClient* Server::getClient(QString ipid)
{
    for (AOClient* client : clients) {
        if (client->getIpid() == ipid)
            return client;
    }
    return nullptr;
}

AOClient* Server::getClientByID(int id)
{
    for (AOClient* client : clients) {
        if (client->id == id)
            return client;
    }
    return nullptr;
}

int Server::getCharID(QString char_name)
{
    for (QString character : characters) {
        if (character.toLower() == char_name.toLower()) {
            return characters.indexOf(QRegExp(character, Qt::CaseInsensitive));
        }
    }
    return -1; // character does not exist
}

Server::~Server()
{
    for (AOClient* client : clients) {
        client->deleteLater();
    }
    server->deleteLater();
    proxy->deleteLater();

    delete db_manager;
}
