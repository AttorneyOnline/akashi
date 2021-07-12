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
#ifndef MASTER_H
#define MASTER_H

#include "include/packet/aopacket.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

/**
 * @brief A communicator class to update the master server on the server's status.
 *
 * @see https://github.com/AttorneyOnline/docs/blob/master/docs/development/network.md#master-server-protocol
 * for more explanation about how to communicate with the master server.
 */
class Advertiser : public QObject {
    Q_OBJECT

  public:
    /**
     * @brief Constructor for the Advertiser class.
     *
     * @param p_ip The IP of the master server.
     * @param p_port The port on which the connection to the master server should be established.
     * @param p_ws_port The port on which the server will accept connections from clients through WebSocket.
     * @param p_local_port The port on which the server will accept connections from clients through TCP.
     * @param p_name The name of the server, as reported in the client's server browser.
     * @param p_description The description of the server, as reported in the client's server browser.
     * @param p_parent Qt-based parent, passed along to inherited constructor from QObject.
     */
    Advertiser(const QString p_ip, const int p_port, const int p_ws_port, const int p_local_port,
               const QString p_name, const QString p_description, QObject* p_parent = nullptr) :
        QObject(p_parent),
        ip(p_ip),
        port(p_port),
        ws_port(p_ws_port),
        local_port(p_local_port),
        name(p_name),
        description(p_description)
    {};

    /**
     * @brief Destructor for the Advertiser class.
     *
     * @details Marks the socket used to establish connection to the master server to be deleted later.
     */
    ~Advertiser();

    /**
     * @brief Sets up the socket used for master server connection, establishes connection to the master server.
     */
    void contactMasterServer();

  public slots:
    /**
     * @brief Handles data that was read from the master server's socket.
     *
     * @note Currently does nothing.
     */
    void readData();

    /**
     * @brief Announces the server's presence to the master server.
     */
    void socketConnected();

    /**
     * @brief Handles disconnection from the master server through the socket.
     *
     * @note Currently does nothing but outputs a line about the disconnection in the debug output.
     */
    void socketDisconnected();

    /**
     * @brief Handles updating the advertiser and recontacting the master server.
     *
     * @param p_name The new server name.
     * @param p_desc The new server description.
     */
    void reloadRequested(QString p_name, QString p_desc);

  private:
    /**
     * @copydoc ConfigManager::server_settings::ms_ip
     */
    QString ip;

    /**
     * @copydoc ConfigManager::server_settings::ms_port
     */
    int port;

    /**
     * @copydoc ConfigManager::server_settings::ws_port
     */
    int ws_port;

    /**
     * @copydoc ConfigManager::server_settings::port
     *
     * @bug See #port.
     */
    int local_port;

    /**
     * @copydoc ConfigManager::server_settings::name
     */
    QString name;

    /**
     * @copydoc ConfigManager::server_settings::description
     */
    QString description;

    /**
     * @brief The socket used to establish connection to the master server.
     */
    QTcpSocket* socket;
};

#endif // MASTER_H
