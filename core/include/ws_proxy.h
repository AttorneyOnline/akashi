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
#ifndef WS_PROXY_H
#define WS_PROXY_H

#include <QHostAddress>
#include <QMap>
#include <QTcpSocket>
#include <QtWebSockets/QtWebSockets>

class WSClient;

/**
 * @brief Handles WebSocket connections by redirecting data sent through them through a local TCP connection
 * for common handling.
 */
class WSProxy : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Creates a WSProxy instance.
     *
     * @param p_local_port The port through which the TCP connection should be directed. Should the same as with
     * non-WebAO connections.
     * @param p_ws_port The WebSocket port. Should the same that is opened for WebSockets connections.
     * @param parent Qt-based parent, passed along to inherited constructor from QObject.
     */
    WSProxy(int p_local_port, int p_ws_port, QObject *parent);

    /**
     * @brief Destructor for the WSProxy class.
     *
     * @details Marks the WebSocket server that is used to handle the proxy process to be deleted later.
     */
    ~WSProxy();

    /**
     * @brief Starts listening for WebSocket connections on the given port.
     */
    void start();

  public slots:
    /**
     * @brief Sets up the proxy process to the newly connected WebSocket.
     *
     * @details This function creates a TCP socket to establish the proxy, creates a WSClient to represent the client connecting through WebSocket.
     */
    void wsConnected();

  private:
    /**
     * @brief The WebSocket server listening to incoming WebSocket connections.
     */
    QWebSocketServer *server;

    /**
     * @brief Every client connected through WebSocket.
     */
    QVector<WSClient *> clients;

    /**
     * @brief The TCP port that the WebSocket connections will be redirected through.
     *
     * @note Should be the same that desktop clients connect through, and that was announced to the master server.
     */
    int local_port;

    /**
     * @brief The port for incoming WebSocket connections.
     *
     * @note Should be the same that was announced to the master server.
     */
    int ws_port;
};

#endif // WS_PROXY_H
