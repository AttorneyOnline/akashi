#include "packet/packet_ms.h"
#include "config_manager.h"
#include "packet/data/msdata.h"
#include "packet/packet_factory.h"
#include "server.h"

#include <QDebug>
#include <QJsonDocument>
#include <QRegularExpression>

PacketMS::PacketMS(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketMS::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 1,
        .header = "MS"};
    return info;
}

void PacketMS::handlePacket(AreaData *area, AOClient &client) const
{
    if (client.m_is_muted) {
        client.sendServerMessage("You cannot speak while muted.");
        return;
    }

    if (!area->isMessageAllowed() || !client.getServer()->isMessageAllowed()) {
        return;
    }

    AOPacket *validated_packet = validateIcPacket(client);
    if (validated_packet->getPacketInfo().header == "INVALID")
        return;

    if (client.m_pos != "")
        validated_packet->setContentField(5, client.m_pos);

    client.getServer()->broadcast(validated_packet, client.areaId());
    emit client.logIC((client.character() + " " + client.characterName()), client.name(), client.m_ipid, client.getServer()->getAreaById(client.areaId())->name(), client.m_last_message);
    area->updateLastICMessage(validated_packet->getContent());

    area->startMessageFloodguard(ConfigManager::messageFloodguard());
    client.getServer()->startMessageFloodguard(ConfigManager::globalMessageFloodguard());
}

AOPacket *PacketMS::validateIcPacket(AOClient &client) const
{
    // Welcome to the super cursed server-side IC chat validation hell

    // I wanted to use enums or #defines here to make the
    // indicies of the args arrays more readable. But,
    // in typical AO fasion, the indicies for the incoming
    // and outgoing packets are different. Just RTFM.

    // This packet can be sent with a minimum required args of 15.
    // 2.6+ extensions raise this to 19, and 2.8 further raises this to 26.

    AOPacket *l_invalid = PacketFactory::createPacket("INVALID", {});
    if (client.isSpectator() || client.character().isEmpty() || !client.m_joined)
        // Spectators cannot use IC
        return l_invalid;
    AreaData *area = client.getServer()->getAreaById(client.areaId());
    if (area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !area->invited().contains(client.clientId()) && !client.checkPermission(ACLRole::BYPASS_LOCKS))
        // Non-invited players cannot speak in spectatable areas
        return l_invalid;

    const auto l_incoming = QJsonDocument::fromJson(m_content.at(0).toUtf8());
    ms2::OldMSFlatData message;
    if (ms2::OldMSFlatData::fromJson(l_incoming.object(), message)) {
        return l_invalid;
    }

    {
        // char name
        if (client.character().toLower() != message.m_char_name.toLower()) {
            // Selected char is different from supplied folder name
            // This means the user is INI-swapped
            if (!area->iniswapAllowed()) {
                QStringList l_character_split = message.m_char_name.split("/");
                if (!client.getServer()->getCharacters().contains(l_character_split.at(0), Qt::CaseInsensitive) || l_character_split.contains(".."))
                    return l_invalid;
            }
            qDebug() << "INI swap detected from " << client.getIpid();
        }
        client.m_current_iniswap = message.m_char_name;
    }
    {
        // emote
        client.m_emote = message.m_emote;
        if (client.m_first_person)
            client.m_emote = "";
        message.m_emote = client.m_emote;
    }
    {
        // message text
        if (message.m_message_text.size() > ConfigManager::maxCharacters())
            return l_invalid;

        // Doublepost prevention. Has to ignore blankposts and testimony commands.
        QString l_incoming_msg = client.dezalgo(message.m_message_text.trimmed());
        QRegularExpressionMatch match = isTestimonyJumpCommand(client.decodeMessage(l_incoming_msg));
        bool msg_is_testimony_cmd = (match.hasMatch() || l_incoming_msg == ">" || l_incoming_msg == "<");
        if (!client.m_last_message.isEmpty()           // If the last message you sent isn't empty,
            && l_incoming_msg == client.m_last_message // and it matches the one you're sending,
            && !msg_is_testimony_cmd)                  // and it's not a testimony command,
            return l_invalid;                          // get it the hell outta here!

        if (l_incoming_msg == "" && area->blankpostingAllowed() == false) {
            client.sendServerMessage("Blankposting has been forbidden in this area.");
            return l_invalid;
        }

        if (!ConfigManager::filterList().isEmpty()) {
            foreach (const QString &regex, ConfigManager::filterList()) {
                QRegularExpression re(regex, QRegularExpression::CaseInsensitiveOption);
                l_incoming_msg.replace(re, "❌");
            }
        }

        if (client.m_is_gimped) {
            QString l_gimp_message = ConfigManager::gimpList().at((client.genRand(1, ConfigManager::gimpList().size() - 1)));
            l_incoming_msg = l_gimp_message;
        }

        if (client.m_is_shaken) {
            QStringList l_parts = l_incoming_msg.split(" ");

            std::random_device rng;
            std::mt19937 urng(rng());
            std::shuffle(l_parts.begin(), l_parts.end(), urng);

            l_incoming_msg = l_parts.join(" ");
        }

        if (client.m_is_disemvoweled) {
            QString l_disemvoweled_message = l_incoming_msg.remove(QRegularExpression("[AEIOUaeiou]"));
            l_incoming_msg = l_disemvoweled_message;
        }

        client.m_last_message = l_incoming_msg;
        message.m_message_text = l_incoming_msg;
    }
    {
        // side
        // this is validated clientside so w/e
        QString side = area->side();
        if (side.isEmpty()) {
            side = message.m_side;
        }
        message.m_side = side;

        if (client.m_pos != message.m_side) {
            client.m_pos = message.m_side;
            client.m_pos.replace("../", "").replace("..\\", "");
            client.updateEvidenceList(client.getServer()->getAreaById(client.areaId()));
        }
    }
    {
        // char id
        if (message.m_char_id != client.m_char_id)
            return l_invalid;
    }
    {
        // objection modifier
        if (!area->isShoutAllowed()) {
            if (message.m_objection_mod != ms2::ObjectionMod::None) {
                client.sendServerMessage("Shouts have been disabled in this area.");
                message.m_objection_mod = ms2::ObjectionMod::None;
            }
        }
    }
    {
        // evidence
        if (message.m_evidence > area->evidence().length())
            return l_invalid;
    }
    {
        // flipping
        client.m_flipping = message.m_flip;
    }
    {
        // text color
        if (message.m_text_colour < 0 || message.m_text_colour > 11)
            return l_invalid;
    }
    {
        // showname
        QString l_incoming_showname = client.dezalgo(message.m_showname.trimmed());
        if (!(l_incoming_showname == client.character() || l_incoming_showname.isEmpty()) && !area->shownameAllowed()) {
            client.sendServerMessage("Shownames are not allowed in this area!");
            return l_invalid;
        }
        if (l_incoming_showname.length() > 30) {
            client.sendServerMessage("Your showname is too long! Please limit it to under 30 characters");
            return l_invalid;
        }

        // if the raw input is not empty but the trimmed input is, use a single space
        if (l_incoming_showname.isEmpty() && !message.m_showname.isEmpty())
            l_incoming_showname = " ";
        message.m_showname = l_incoming_showname;
        client.setCharacterName(l_incoming_showname);
    }
    {
        // other char id
        client.m_pairing_with = message.m_other_charid;
        QString l_front_back = "";
        int l_other_charid = client.m_pairing_with;
        bool l_pairing = false;
        QString l_other_name = "0";
        QString l_other_emote = "0";
        ms2::OffsetData l_other_offset{0, 0};
        bool l_other_flip = false;
        for (int l_client_id : area->joinedIDs()) {
            AOClient *l_client = client.getServer()->getClientByID(l_client_id);
            if (l_client->m_pairing_with == client.m_char_id && l_other_charid != client.m_char_id && l_client->m_char_id == client.m_pairing_with && l_client->m_pos == client.m_pos) {
                l_other_name = l_client->m_current_iniswap;
                l_other_emote = l_client->m_emote;
                l_other_offset = l_client->m_offset;
                l_other_flip = l_client->m_flipping;
                l_pairing = true;
            }
        }
        if (!l_pairing) {
            l_other_charid = -1;
            l_front_back = "";
        }
        message.m_other_name = l_other_name;
        message.m_other_charid = l_other_charid;
        message.m_other_emote = l_other_emote;
        message.m_other_flip = l_other_flip;
        message.m_other_offset = l_other_offset;
    }
    {
        // self offset
        client.m_offset = message.m_self_offset;
    }
    {
        // immediate text processing
        if (area->forceImmediate()) {
            message.m_immediate = true;
            if (message.m_emote_mod == ms2::EmoteMod::NoPreObjZoom) {
                message.m_emote_mod = ms2::EmoteMod::NoPreZoom;
            }
        }
    }
    {
        // additive
        if (area->lastICMessage().isEmpty()) {
            message.m_additive = false;
        }
        else if (!(client.m_char_id == area->lastICMessage()[8].toInt())) {
            message.m_additive = false;
        }
        else if (message.m_additive == true) {
            message.m_message_text.insert(0, " ");
        }
    }

    // Testimony playback
    QString client_name = client.name();
    if (client_name == "") {
        client_name = client.character(); // fallback in case of empty ooc name
    }
    if (area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING || area->testimonyRecording() == AreaData::TestimonyRecording::ADD) {
        // -1 indicates title
        if (area->statement() == -1) {
            message.m_message_text = "~~-- " + message.m_message_text + " --";
            message.m_text_colour = 3;
            client.getServer()->broadcast(PacketFactory::createPacket("RT", {"testimony1", "0"}), client.areaId());
        }
        client.addStatement(message);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::UPDATE) {
        message = client.updateStatement(message);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::PLAYBACK) {
        AreaData::TestimonyProgress l_progress;

        if (message.m_message_text == ">") {
            auto l_statement = area->jumpToStatement(area->statement() + 1);
            message = l_statement.first;
            l_progress = l_statement.second;
            client.m_pos = message.m_side;

            client.sendServerMessageArea(client_name + " moved to the next statement.");

            if (l_progress == AreaData::TestimonyProgress::LOOPED) {
                client.sendServerMessageArea("Last statement reached. Looping to first statement.");
            }
        }
        if (message.m_message_text == "<") {
            auto l_statement = area->jumpToStatement(area->statement() - 1);
            message = l_statement.first;
            l_progress = l_statement.second;
            client.m_pos = message.m_side;

            client.sendServerMessageArea(client_name + " moved to the previous statement.");

            if (l_progress == AreaData::TestimonyProgress::STAYED_AT_FIRST) {
                client.sendServerMessage("First statement reached.");
            }
        }
        if (message.m_message_text == "=") {
            auto l_statement = area->jumpToStatement(area->statement());
            message = l_statement.first;
            l_progress = l_statement.second;
            client.m_pos = message.m_side;

            client.sendServerMessageArea(client_name + " repeated the current statement.");
        }

        QRegularExpressionMatch match = isTestimonyJumpCommand(client.decodeMessage(message.m_message_text)); // Get rid of that pesky encoding, then do the fun part
        if (match.hasMatch()) {
            int jump_idx = match.captured("int").toInt();
            auto l_statement = area->jumpToStatement(jump_idx);
            message = l_statement.first;
            l_progress = l_statement.second;

            client.sendServerMessageArea(client_name + " jumped to statement number " + QString::number(jump_idx) + ".");

            switch (l_progress) {
            case AreaData::TestimonyProgress::LOOPED:
            {
                client.sendServerMessageArea("Last statement reached. Looping to first statement.");
                break;
            }
            case AreaData::TestimonyProgress::STAYED_AT_FIRST:
            {
                client.sendServerMessage("First statement reached.");
                Q_FALLTHROUGH();
            }
            case AreaData::TestimonyProgress::OK:
            default:
                // No need to handle.
                break;
            }
        }
    }

    return PacketFactory::createPacket("MS", {QJsonDocument{message.toJson()}.toJson()});
}

QRegularExpressionMatch PacketMS::isTestimonyJumpCommand(QString message) const
{
    // *sigh* slightly too chunky and needed slightly
    // too often to justify not making this a helper
    // even if it hurts my heart
    //
    // and my grey matter
    //
    // get well soon
    QRegularExpression jump("(?<arrow>>|<)(?<int>\\d+)");
    return jump.match(message);
}
