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
#ifndef HTTP_ADVERTISER_H
#define HTTP_ADVERTISER_H

#include <QtNetwork>
#include <QObject>


/**
 * @brief Represents the advertiser of the server. Sends current server information to masterserver.
 */
class HTTPAdvertiser : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor for the HTTP_Advertiser class.
     */
    explicit HTTPAdvertiser();

public slots:

    /**
     * @brief msAdvertiseServer Establishes a connection with masterserver to
     *        register or update the listing on the masterserver.
     */
    void msAdvertiseServer();

    /**
     * @brief msRequestFinished Reads the information send as a reply for further
     *        error handling.
     */
    void msRequestFinished(QNetworkReply *reply);

    /**
     * @brief setAdvertiserSettings Configures the values being advertised to masterserver.
     * @param f_name Servername.
     * @param f_description Serverdescription.
     * @param f_port Client port.
     * @param f_ws_port Optional Websocket proxy port.
     * @param f_players Maximum amount of clients.
     * @param f_master_url URL of the advertisement target.
     */
    void setAdvertiserSettings(QString f_name, QString f_description, int f_port, int f_ws_port, int f_players, QUrl f_master_url, bool f_debug);

private:

    /**
     * @brief m_manager NetworkAccessManager for HTTP Advertiser.
     */
    QNetworkAccessManager* m_manager;

    /**
     * @brief m_name Current name of the server.
     */
    QString m_name;

    /**
     * @brief m_description Current description of the server.
     */
    QString m_description;

    /**
     * @brief m_port Current port for AO2-Clients.
     */
    int m_port;

    /**
     * @brief m_ws_port Websocket proxy port for WebAO-Clients.
     */
    int m_ws_port;

    /**
     * @brief m_players Maximum number of connected clients.
     */
    int m_players;

    /**
     * @brief m_masterserver URL of the masterserver being advertised to.
     */
    QUrl m_masterserver;

    /**
     * @brief m_debug If debug information is displayed in console.
     */
    bool m_debug;
};

#endif // HTTP_ADVERTISER_H
