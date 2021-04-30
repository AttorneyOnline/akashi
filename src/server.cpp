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

Server::Server(int p_port, int p_ws_port, QObject* parent) :
    QObject(parent),
    player_count(0),
    port(p_port),
    ws_port(p_ws_port)
{
    server = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(clientConnected()));

    timer = new QTimer();

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

    loadServerConfig();
    loadCommandConfig();
    
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

    int multiclient_count = 1;
    bool is_at_multiclient_limit = false;
    bool is_banned = db_manager->isIPBanned(socket->peerAddress());
    for (AOClient* joined_client : clients) {
        if (client->remote_ip.isEqual(joined_client->remote_ip))
            multiclient_count++;
    }

    if (multiclient_count > multiclient_limit && !client->remote_ip.isLoopback()) // TODO: make this configurable
        is_at_multiclient_limit = true;

    if (is_banned) {
        AOPacket ban_reason("BD", {db_manager->getBanReason(socket->peerAddress())});
        socket->write(ban_reason.toUtf8());
    }
    if (is_banned || is_at_multiclient_limit) {
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
    client->calculateIpid();
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

    for (AOClient* client : clients) {
        if (client->current_area == area->index){
            if (!client->is_charcursed)
                client->sendPacket(response_cc);
            else {
                QStringList chars_taken_cursed = getCursedCharsTaken(client, chars_taken);
                AOPacket response_cc_cursed("CharsCheck", chars_taken_cursed);
                client->sendPacket(response_cc_cursed);
            }
        }
    }
}

QStringList Server::getCursedCharsTaken(AOClient* client, QStringList chars_taken)
{
    QStringList chars_taken_cursed;
    for (int i = 0; i < chars_taken.length(); i++) {
        if (!client->charcurse_list.contains(i))
            chars_taken_cursed.append("-1");
        else
            chars_taken_cursed.append(chars_taken.value(i));
    }
    return chars_taken_cursed;
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

QList<AOClient*> Server::getClientsByIpid(QString ipid)
{
    QList<AOClient*> return_clients;
    for (AOClient* client : clients) {
        if (client->getIpid() == ipid)
            return_clients.append(client);
    }
    return return_clients;
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

void Server::loadCommandConfig()
{
    magic_8ball_answers = (loadConfigFile("8ball"));
    praise_list = (loadConfigFile("praise"));
    reprimands_list = (loadConfigFile("reprimands"));
    gimp_list = (loadConfigFile("gimp"));
}

QStringList Server::loadConfigFile(QString filename)
{
    QStringList stringlist;
    QFile file("config/text/" + filename + ".txt");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!(file.atEnd())) {
        stringlist.append(file.readLine().trimmed());
    }
    file.close();
    return stringlist;
}

void Server::loadServerConfig()
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    config.beginGroup("Options");
    //Load config.ini values
    max_players = config.value("max_players","100").toString();
    server_name = config.value("server_name","An Unnamed Server").toString();
    server_desc = config.value("server_description","This is a placeholder server description. Tell the world of AO who you are here!").toString();
    MOTD = config.value("motd","MOTD is not set.").toString();
    auth_type = config.value("auth","simple").toString();
    modpass = config.value("modpass","").toString();
    bool maximum_statements_conversion_success;
    maximum_statements = config.value("maximum_statements", "10").toInt(&maximum_statements_conversion_success);
    if (!maximum_statements_conversion_success)
        maximum_statements = 10;
    bool afk_timeout_conversion_success;
    afk_timeout = config.value("afk_timeout", "300").toInt(&afk_timeout_conversion_success);
    if (!afk_timeout_conversion_success)
        afk_timeout = 300;
    bool multiclient_limit_conversion_success;
    multiclient_limit = config.value("multiclient_limit", "15").toInt(&multiclient_limit_conversion_success);
    if (!multiclient_limit_conversion_success)
        multiclient_limit = 15;
    bool max_char_conversion_success;
    max_chars = config.value("maximum_characters", "256").toInt(&max_char_conversion_success);
    if (!max_char_conversion_success)
        max_chars = 256;
    config.endGroup();

    //Load dice values
    config.beginGroup("Dice");
    dice_value = config.value("value_type", "100").toInt();
    max_dice = config.value("max_dice","100").toInt();
    config.endGroup();
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
