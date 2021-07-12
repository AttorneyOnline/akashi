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
#include "include/aoclient.h"

void AOClient::pktDefault(AreaData* area, int argc, QStringList argv, AOPacket packet)
{
#ifdef NET_DEBUG
    qDebug() << "Unimplemented packet:" << packet.header << packet.contents;
#endif
}

void AOClient::sendEvidenceList(AreaData* area)
{
    for (AOClient* client : server->clients) {
        if (client->current_area == current_area)
            client->updateEvidenceList(area);
    }
}

void AOClient::updateEvidenceList(AreaData* area)
{
    QStringList evidence_list;
    QString evidence_format("%1&%2&%3");

    for (AreaData::Evidence evidence : area->evidence()) {
        if (!checkAuth(ACLFlags.value("CM")) && area->eviMod() == AreaData::EvidenceMod::HIDDEN_CM) {
            QRegularExpression regex("<owner=(.*?)>");
            QRegularExpressionMatch match = regex.match(evidence.description);
            if (match.hasMatch()) {
                QStringList owners = match.captured(1).split(",");
                if (!owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) && !owners.contains(pos, Qt::CaseSensitivity::CaseInsensitive)) {
                    continue;
                }
            }
            // no match = show it to all
        }
        evidence_list.append(evidence_format
            .arg(evidence.name)
            .arg(evidence.description)
            .arg(evidence.image));
    }

    sendPacket(AOPacket("LE", evidence_list));
}


QString AOClient::dezalgo(QString p_text)
{
    QRegularExpression rxp("([̴̵̶̷̸̡̢̧̨̛̖̗̘̙̜̝̞̟̠̣̤̥̦̩̪̫̬̭̮̯̰̱̲̳̹̺̻̼͇͈͉͍͎̀́̂̃̄̅̆̇̈̉̊̋̌̍̎̏̐̑̒̓̔̽̾̿̀́͂̓̈́͆͊͋͌̕̚ͅ͏͓͔͕͖͙͚͐͑͒͗͛ͣͤͥͦͧͨͩͪͫͬͭͮͯ͘͜͟͢͝͞͠͡])");
    QString filtered = p_text.replace(rxp, "");
    return filtered;
}

bool AOClient::checkEvidenceAccess(AreaData *area)
{
    switch(area->eviMod()) {
    case AreaData::EvidenceMod::FFA:
        return true;
    case AreaData::EvidenceMod::CM:
    case AreaData::EvidenceMod::HIDDEN_CM:
        return checkAuth(ACLFlags.value("CM"));
    case AreaData::EvidenceMod::MOD:
        return authenticated;
    default:
        return false;
    }
}

void AOClient::updateJudgeLog(AreaData* area, AOClient* client, QString action)
{
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    QString uid = QString::number(client->id);
    QString char_name = client->current_char;
    QString ipid = client->getIpid();
    QString message = action;
    QString logmessage = QString("[%1]: [%2] %3 (%4) %5").arg(timestamp, uid, char_name, ipid, message);
    area->appendJudgelog(logmessage);
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
            sendPacket("AUTH", {"1"}); // Client: "You were granted the Disable Modcalls button."
            sendServerMessage("Logged in as a moderator."); // pre-2.9.1 clients are hardcoded to display the mod UI when this string is sent in OOC
            authenticated = true;
        }
        else {
            sendPacket("AUTH", {"0"}); // Client: "Login unsuccessful."
            sendServerMessage("Incorrect password.");
        }
        server->areas.value(current_area)->logLogin(current_char, ipid, authenticated, "moderator");
        break;
    case DataTypes::AuthType::ADVANCED:
        QStringList login = message.split(" ");
        if (login.size() < 2) {
            sendServerMessage("You must specify a username and a password");
            sendServerMessage("Exiting login prompt.");
            is_logging_in = false;
            return;
        }
        QString username = login[0];
        QString password = login[1];
        if (server->db_manager->authenticate(username, password)) {
            moderator_name = username;
            authenticated = true;
            sendPacket("AUTH", {"1"}); // Client: "You were granted the Disable Modcalls button."
            if (version.release <= 2 && version.major <= 9 && version.minor <= 0)
                sendServerMessage("Logged in as a moderator."); // pre-2.9.1 clients are hardcoded to display the mod UI when this string is sent in OOC
            sendServerMessage("Welcome, " + username);
        }
        else {
            sendPacket("AUTH", {"0"}); // Client: "Login unsuccessful."
            sendServerMessage("Incorrect password.");
        }
        server->areas.value(current_area)->logLogin(current_char, ipid, authenticated, username);
        break;
    }
    sendServerMessage("Exiting login prompt.");
    is_logging_in = false;
    return;
}

