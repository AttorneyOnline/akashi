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
//    but WITHOUT ANY WARRANTY{} without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "aoclient.h"
#include "proto/packet.h"
#include "proto/text_utils.h"

#include <QQueue>

#include "area_data.h"
#include "config_manager.h"
#include "db_manager.h"
#include "music_manager.h"
#include "proto/evidence.h"
#include "server.h"

void AOClient::sendEvidenceList(AreaData *area) const
{
    const QVector<AOClient *> l_clients = m_server->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->areaId() == areaId())
            l_client->updateEvidenceList(area);
    }
}

void AOClient::updateEvidenceList(AreaData *area)
{
    QStringList l_evidence_list;

    const QList<AreaData::Evidence> l_area_evidence = area->evidence();
    for (const AreaData::Evidence &evidence : l_area_evidence) {
        if (!canPerform(ACLRole::CM) && area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM) {
            QRegularExpression l_regex("<owner=(.*?)>");
            QRegularExpressionMatch l_match = l_regex.match(evidence.description);
            if (l_match.hasMatch()) {
                QStringList owners = l_match.captured(1).split(",");
                if (!owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) && !owners.contains(m_player.pos, Qt::CaseSensitivity::CaseInsensitive)) {
                    continue;
                }
            }
            // no match = show it to all
        }
        l_evidence_list.append(akashi::Evidence{evidence.name, evidence.description, evidence.image}.toLeField());
    }

    sendPacket(akashi::Packet("LE", l_evidence_list));
}

QString AOClient::dezalgo(QString p_text)
{
    return akashi::stripZalgo(p_text);
}

bool AOClient::canModifyEvidence(AreaData *area)
{
    switch (area->eviMod()) {
    case AreaData::EvidenceMod::FFA:
        return true;
    case AreaData::EvidenceMod::CM:
    case AreaData::EvidenceMod::HIDDEN_CM:
        return canPerform(ACLRole::CM);
    case AreaData::EvidenceMod::MOD:
        return m_session.authenticated;
    default:
        return false;
    }
}

void AOClient::updateJudgeLog(AreaData *area, AOClient *client, QString action)
{
    QString l_timestamp = QTime::currentTime().toString("hh:mm:ss");
    QString l_uid = QString::number(client->clientId());
    QString l_char_name = client->character();
    QString l_ipid = client->ipid();
    QString l_message = action;
    QString l_logmessage = QString("[%1]: [%2] %3 (%4) %5").arg(l_timestamp, l_uid, l_char_name, l_ipid, l_message);
    area->appendJudgelog(l_logmessage);
}

QString AOClient::decodeMessage(QString incoming_message)
{
    QString decoded_message = incoming_message.replace("<num>", "#")
                                  .replace("<percent>", "%")
                                  .replace("<dollar>", "$")
                                  .replace("<and>", "&");
    return decoded_message;
}

void AOClient::loginAttempt(QString message)
{
    switch (ConfigManager::authType()) {
    case DataTypes::AuthType::SIMPLE:
        if (message == ConfigManager::modpass()) {
            sendPacket("AUTH", {"1"});
            if (m_session.profile.version.release <= 2 && m_session.profile.version.major <= 9 && m_session.profile.version.minor <= 0)
                sendServerMessage("Logged in as a moderator.");
            m_session.authenticated = true;
            m_session.acl_role_id = ACLRolesHandler::SUPER_ID;
        }
        else {
            sendPacket("AUTH", {"0"}); // Client: "Login unsuccessful."
            sendServerMessage("Incorrect password.");
        }
        Q_EMIT logLogin((character() + " " + characterName()), name(), "Moderator",
                      m_session.ipid, m_server->areaById(areaId())->name(), m_session.authenticated);
        break;
    case DataTypes::AuthType::ADVANCED:
        QStringList l_login = message.split(" ");
        if (l_login.size() < 2) {
            sendServerMessage("You must specify a username and a password");
            sendServerMessage("Exiting login prompt.");
            m_session.logging_in = false;
            return;
        }
        QString username = l_login[0];
        QString password = l_login[1];
        if (m_server->databaseManager()->authenticate(username, password)) {
            m_session.authenticated = true;
            m_session.acl_role_id = m_server->databaseManager()->acl(username);
            m_session.moderator_name = username;
            sendPacket("AUTH", {"1"});
            if (m_session.profile.version.release <= 2 && m_session.profile.version.major <= 9 && m_session.profile.version.minor <= 0)
                sendServerMessage("Logged in as a moderator.");
            sendServerMessage("Welcome, " + username);
        }
        else {
            sendPacket("AUTH", {"0"});
            sendServerMessage("Incorrect password.");
        }
        Q_EMIT logLogin((character() + " " + characterName()), name(), username, m_session.ipid,
                      m_server->areaById(areaId())->name(), m_session.authenticated);
        break;
    }
    sendServerMessage("Exiting login prompt.");
    m_session.logging_in = false;
    return;
}
