//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                           //
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
#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QtWebSockets/QtWebSockets>

/**
 * @brief Represents a WebSocket client (generally WebAO) connected to the server.
 *
 * @details To give a common interface to both desktop AO and WebAO clients, the incoming data from
 * WebSocket connections are directed through local TCP sockets.
 *
 * This class is a very thin layer -- see WSProxy for the actual mechanics of this WebSocket-to-TCP proxy solution.
 */
class WSClient : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Creates an instance of the WSClient class.
     *
     * @param p_tcp_socket The locally created TCP socket to direct data through.
     * @param p_web_socket The WebSocket that actually represents the connecting client.
     * @param parent Qt-based parent, passed along to inherited constructor from QObject.
     *
     * @pre This class will not connect up the ports to each other in any way. Unless some setup is done, this class
     * by default will never be prompted to read and/or write from/to either of the sockets.
     */
    WSClient(QTcpSocket *p_tcp_socket, QWebSocket *p_web_socket, QObject *parent = nullptr);

    /**
     * @brief Destructor for the WSClient class.
     *
     * @details Marks the TCP and WebSocket for later deletion.
     */
    ~WSClient();
  public slots:
    /**
     * @brief A slot that can be signalled when #tcp_socket has data ready for reading.
     * Will read all data in the socket.
     *
     * @details The incoming data is separated per-packet due to the WebAO bug, and the packets are sent
     * through #web_socket.
     */
    void onTcpData();

    /**
     * @brief A slot that can be signalled to push packets received from WebSocket into the
     * associated local TCP socket.
     *
     * @param message The incoming packet.
     */
    void onWsData(QString message);

    /**
     * @brief A slot that can be signalled when the WebSocket client disconnect.
     * Disconnects the associated TCP socket.
     *
     * @see onTcpDisconnect() for the opposite scenario.
     */
    void onWsDisconnect();

    /**
     * @brief A slot that can be signalled when the TCP socket is disconnected.
     * Severs the connection to the WebSocket.
     *
     * @see onWsDisconnect() for the opposite scenario.
     */
    void onTcpDisconnect();

    /**
     * @brief A slot that can be signalled when the TCP socket is connected.
     * Sends identification over the socket.
     */
    void onTcpConnect();

  private:
    /**
     * @brief The local TCP socket used as a proxy to connect with the server.
     */
    QTcpSocket *tcp_socket;

    /**
     * @brief The WebSocket representing an incoming connection.
     */
    QWebSocket *web_socket;

    /**
     * @brief Stores partial packets in case they don't all come through the TCP socket at once
     */
    QByteArray partial_packet;

    /**
     * @brief Flag that is set when packets are segmented
     */
    bool is_segmented = false;

    /**
     * @brief The IP send in the WSIP packet
     */
    QString websocket_ip;
};

#endif // WS_CLIENT_H
