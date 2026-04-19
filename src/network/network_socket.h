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
#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include "akashi_core_export.h"
#include "core/transport.h"

#include <QHostAddress>
#include <QNetworkRequest>
#include <QWebSocket>

#include "proto/packet.h"

// The WebSocket transport: the one ITransport implementation core ships. Keeps
// the WebSocket framing and reverse-proxy IP resolution; the rest of the server
// only sees the akashi::ITransport interface.
class AKASHI_CORE_EXPORT NetworkSocket : public akashi::ITransport
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor for the network socket class.
     * @param QWebSocket for communication with external AO2-Client or WebAO clients.
     * @param Pointer to the server object.
     */
    NetworkSocket(QWebSocket *f_socket, QObject *parent = nullptr);

    /**
     * @brief Default destructor for the NetworkSocket object.
     */
    ~NetworkSocket();

    QHostAddress peerAddress() const override;
    void write(const akashi::Packet &f_packet) override;
    void close() override;
    akashi::ITransport::Capabilities capabilities() const override;
    QStringList connectTimeFeatures() const override;

    /**
     * @brief Reads the client's announced capabilities out of the upgrade
     * request - the FL exchange's other direction, carried in the header.
     *
     * @details The subprotocol offer list is the client's feature list.
     * A network_-prefixed token becomes a feature under the same name FL
     * uses ("network_auth_password" reads as "auth_password"), and a
     * client-assembled packet key like "ao_ms_2.11.1" is kept whole. The
     * response may echo only one token (the first offered one the server
     * speaks), so the echo is an acceptance mark, not the exchange itself.
     * Static and pure so it can be tested directly.
     */
    static QStringList parseCapabilityTokens(const QNetworkRequest &f_request);

  private Q_SLOTS:
    /**
     * @brief Splits incoming WebSocket data into packets.
     */
    void handleMessage(QString f_data);

  private:
    QWebSocket *m_client_socket;

    /**
     * @brief Remote IP of the client.
     *
     * @details In the case of the WebSocket we also check if this has been proxy forwarded.
     */
    QHostAddress m_socket_ip;

    /**
     * @brief The capabilities the client announced in the upgrade request.
     */
    QStringList m_connect_features;
};

#endif
