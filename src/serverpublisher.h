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
#pragma once

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

/**
 * @brief Represents the ServerPublisher of the server. Sends current server information to the serverlist.
 */
class ServerPublisher : public QObject
{
    Q_OBJECT

  public:
    explicit ServerPublisher(int port, int *player_count, QObject *parent = nullptr);
    virtual ~ServerPublisher(){};

  public slots:

    /**
     * @brief Establishes a connection with masterserver to register or update the listing on the masterserver.
     */
    void publishServer();

    /**
     * @brief Reads the response from the serverlist.
     */
    void finished(QNetworkReply *f_reply);

  private:
    /**
     * @brief Pointer to the network manager, necessary to execute POST requests to the masterserver.
     */
    QNetworkAccessManager *m_manager;

    /**
     * @brief Advertisers when it expires.
     */
    QTimer *timeout_timer;

    /**
     * @brief The current amount of players on the server.
     */
    int *m_players;

    /**
     * @brief The WS port of the server.
     */
    int m_port;
};
