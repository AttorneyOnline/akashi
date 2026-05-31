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
#include "akashi/log_event.h"
#include "area_data.h"
#include "core/auth_throttle.h"
#include "core/client_session.h"
#include "core/log_service.h"
#include "core/server_settings.h"
#include "db_manager.h"
#include "proto/evidence.h"
#include "proto/packet.h"
#include "proto/text_utils.h"
#include "server.h"

#include <QFutureWatcher>
#include <QPointer>
#include <QQueue>
#include <QtConcurrent/QtConcurrentRun>

namespace log_type = akashi::log_type;

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

    // The store applies the hidden-items rules; the same filter also drives
    // the visible-index translation, so the two can never disagree.
    const QList<akashi::Evidence> l_visible = area->visibleEvidence(canPerform(ACLRole::CM), player()->pos);
    for (const akashi::Evidence &l_item : l_visible) {
        l_evidence_list.append(l_item.toLeField());
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
        return m_session->authenticated;
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
    akashi::AuthThrottle *l_throttle = m_server->authThrottle();
    if (l_throttle->isLockedOut(m_session->ipid)) {
        sendServerMessage("Too many failed login attempts. Try again in "
                          + QString::number(l_throttle->remainingLockoutSeconds(m_session->ipid))
                          + " seconds.");
        sendServerMessage("Exiting login prompt.");
        m_session->logging_in = false;
        return;
    }

    switch (m_server->authType()) {
    case DataTypes::AuthType::SIMPLE:
        if (message == m_server->serverSettings()->modpass()) {
            sendPacket("AUTH", {"1"});
            if (m_session->profile.version.release <= 2 && m_session->profile.version.major <= 9 && m_session->profile.version.minor <= 0)
                sendServerMessage("Logged in as a moderator.");
            m_session->authenticated = true;
            m_session->acl_role_id = ACLRolesHandler::SUPER_ID;
            l_throttle->recordSuccess(m_session->ipid);
        }
        else {
            sendPacket("AUTH", {"0"});
            sendServerMessage("Incorrect password.");
            l_throttle->recordFailure(m_session->ipid);
        }
        m_server->logService()->log({.type = log_type::Login,
            .area = m_server->areaById(areaId())->name(),
            .char_name = character() + " " + characterName(),
            .ooc_name = name(),
            .ipid = m_session->ipid,
            .moderator = QStringLiteral("Moderator"),
            .success = m_session->authenticated});
        break;
    case DataTypes::AuthType::ADVANCED: {
        QStringList l_login = message.split(" ");
        if (l_login.size() < 2) {
            sendServerMessage("You must specify a username and a password");
            sendServerMessage("Exiting login prompt.");
            m_session->logging_in = false;
            return;
        }
        QString l_username = l_login[0];
        QString l_password = l_login[1];

        auto l_creds = m_server->databaseManager()->fetchCredentials(l_username);
        if (!l_creds) {
            sendPacket("AUTH", {"0"});
            sendServerMessage("Incorrect password.");
            l_throttle->recordFailure(m_session->ipid);
            m_server->logService()->log({.type = log_type::Login,
                .area = m_server->areaById(areaId())->name(),
                .char_name = character() + " " + characterName(),
                .ooc_name = name(),
                .ipid = m_session->ipid,
                .moderator = l_username,
                .success = false});
            break;
        }

        sendServerMessage("Exiting login prompt.");
        m_session->logging_in = false;

        QString l_salt = l_creds->salt;
        QString l_stored_hash = l_creds->stored_hash;
        QString l_acl_role = l_creds->acl_role;
        bool l_needs_rehash = QByteArray::fromHex(l_salt.toUtf8()).length() < CryptoHelper::pbkdf2_salt_len;

        QFuture<QString> l_future = QtConcurrent::run([l_salt, l_password]() {
            return CryptoHelper::hash_password(QByteArray::fromHex(l_salt.toUtf8()), l_password);
        });

        auto *l_watcher = new QFutureWatcher<QString>(this);
        QPointer<AOClient> l_guard(this);
        connect(l_watcher, &QFutureWatcher<QString>::finished, this,
            [l_guard, l_watcher, l_username, l_password, l_stored_hash, l_acl_role, l_needs_rehash]() {
                l_watcher->deleteLater();
                if (!l_guard)
                    return;
                AOClient *l_self = l_guard.data();

                const QString l_computed = l_watcher->result();
                const bool l_matches = CryptoHelper::constantTimeEquals(l_computed, l_stored_hash);

                akashi::AuthThrottle *l_throttle = l_self->m_server->authThrottle();
                if (l_matches) {
                    l_self->m_session->authenticated = true;
                    l_self->m_session->acl_role_id = l_acl_role;
                    l_self->m_session->moderator_name = l_username;
                    l_self->sendPacket("AUTH", {"1"});
                    if (l_self->m_session->profile.version.release <= 2 && l_self->m_session->profile.version.major <= 9 && l_self->m_session->profile.version.minor <= 0)
                        l_self->sendServerMessage("Logged in as a moderator.");
                    l_self->sendServerMessage("Welcome, " + l_username);
                    l_throttle->recordSuccess(l_self->m_session->ipid);

                    if (l_needs_rehash)
                        l_self->m_server->databaseManager()->updatePassword(l_username, l_password);
                }
                else {
                    l_self->sendPacket("AUTH", {"0"});
                    l_self->sendServerMessage("Incorrect password.");
                    l_throttle->recordFailure(l_self->m_session->ipid);
                }

                l_self->m_server->logService()->log({.type = log_type::Login,
                    .area = l_self->m_server->areaById(l_self->areaId())->name(),
                    .char_name = l_self->character() + " " + l_self->characterName(),
                    .ooc_name = l_self->name(),
                    .ipid = l_self->m_session->ipid,
                    .moderator = l_username,
                    .success = l_self->m_session->authenticated});
            });
        l_watcher->setFuture(l_future);
        return;
    }
    }
    sendServerMessage("Exiting login prompt.");
    m_session->logging_in = false;
    return;
}
