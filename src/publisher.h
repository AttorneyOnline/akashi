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
#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <QObject>
#include <QTimer>

#include "akashidefs.h"

class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief Represents the Publisher of the server. Sends current server information to masterserver.
 */
class Publisher : public QObject
{
    Q_OBJECT

  public:
    explicit Publisher(akashi::PublisherInfo *config, QObject *parent);
    ~Publisher(){};

    static const int TIMEOUT_TIME = 1000 * 60 * 5;

  public slots:
    /**
     * @brief Establishes a connection with masterserver to register or update the listing on the masterserver.
     */
    void publishServerInfo();

    /**
     * @brief Reads the information send as a reply for further error handling.
     * @param reply Response data from the masterserver. Information contained is send to the console if debug is enabled.
     */
    void finished(QNetworkReply *f_reply);

  private:
    QTimer timeout_timer;
    akashi::PublisherInfo *m_config;
    QNetworkAccessManager *m_serverlist;
};

#endif // PUBLISHER_H
