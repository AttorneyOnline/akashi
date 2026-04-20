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
#include "proto/packet.h"

#include <QHostAddress>
#include <QNetworkRequest>
#include <QWebSocket>

class QTimer;

// The WebSocket transport: the one ITransport implementation core ships. Keeps
// the WebSocket framing and reverse-proxy IP resolution; the rest of the server
// only sees the akashi::ITransport interface.
//
// It enforces the ITransport lifecycle contract itself: the peer is pinged and
// aborted when it stops answering (catches half-open TCP and frozen clients),
// a close() that the peer never completes is aborted after a grace period, and
// clientDisconnected() is emitted exactly once whichever way the wire dies.
class AKASHI_CORE_EXPORT NetworkSocket : public akashi::ITransport
{
    Q_OBJECT

  public:
    // Takes ownership of the QWebSocket.
    explicit NetworkSocket(QWebSocket *f_socket, QObject *parent = nullptr);

    QHostAddress peerAddress() const override;
    void write(const akashi::Packet &f_packet) override;
    void close() override;
    bool isOpen() const override;
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

    /**
     * @brief Forwards the wire-level disconnect as exactly one clientDisconnected().
     */
    void reportDisconnect();

    /**
     * @brief Pings the peer; aborts a peer that stopped answering.
     */
    void checkLiveness();

  private:
    /**
     * @brief Starts the WebSocket close exchange, aborting if the peer never finishes it.
     */
    void closeWithCode(QWebSocketProtocol::CloseCode f_code);

    /**
     * @brief Drops the connection immediately and reports the disconnect.
     */
    void abortConnection();

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

    /**
     * @brief Guards the exactly-once clientDisconnected() emission.
     */
    bool m_disconnect_reported = false;

    /**
     * @brief Set once close() is called; inbound frames are dropped from then on.
     */
    bool m_closing = false;

    /**
     * @brief Pings sent since the last pong came back.
     */
    int m_unanswered_pings = 0;

    QTimer *m_liveness_timer;
};

#endif
