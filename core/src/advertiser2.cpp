#include "include/advertiser2.h"

Advertiser2::Advertiser2()
{
    m_manager = new QNetworkAccessManager();
}

void Advertiser2::advertiseServer()
{
    if (m_masterserver.isValid()){
        qDebug() << "Unable to advertise. Masterserver URL is not valid.";
        return;
    }
}

void Advertiser2::readReply(QNetworkReply *reply)
{

}

void Advertiser2::setAdvertiserSettings(QString f_name, QString f_description, int f_port, int f_ws_port, int f_players, QUrl f_master_url)
{

}
