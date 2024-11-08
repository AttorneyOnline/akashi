#include "packet/packet_ms.h"
#include "config_manager.h"
#include "packet/packet_factory.h"
#include "server.h"

#include <QDebug>
#include <QRegularExpression>

enum MS_FIELD
{
    MS_DESK_MOD,
    MS_PRE_EMOTE,
    MS_CHAR_NAME,
    MS_EMOTE,
    MS_MESSAGE,
    MS_SIDE,
    MS_SFX_NAME,
    MS_EMOTE_MOD,
    MS_CHAR_ID,
    MS_SFX_DELAY,
    MS_OBJECTION_MOD,
    MS_EVIDENCE_ID,
    MS_FLIP,
    MS_REALIZATION,
    MS_TEXT_COLOR,
    MS_SHOWNAME,
    MS_OTHER_CHARID,
    MS_OTHER_NAME,
    MS_OTHER_EMOTE,
    MS_SELF_OFFSET,
    MS_OTHER_OFFSET,
    MS_OTHER_FLIP,
    MS_IMMEDIATE,
    MS_LOOPING_SFX,
    MS_SCREENSHAKE,
    MS_FRAME_SCREENSHAKE,
    MS_FRAME_REALIZATION,
    MS_FRAME_SFX,
    MS_ADDITIVE,
    MS_EFFECTS,
    MS_BLIPNAME,
    MS_SLIDE,
    MAX_SIZE_MS,
};

PacketMS::PacketMS(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketMS::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = MAX_SIZE_MS,
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
    AOPacket *l_invalid = PacketFactory::createPacket("INVALID", {});
    QStringList l_args;
    l_args.resize(MAX_SIZE_MS);

    if (client.isSpectator() || client.character().isEmpty() || !client.m_joined)
        // Spectators cannot use IC
        return l_invalid;
    AreaData *area = client.getServer()->getAreaById(client.areaId());
    if (area->lockStatus() == AreaData::LockStatus::SPECTATABLE && !area->invited().contains(client.clientId()) && !client.checkPermission(ACLRole::BYPASS_LOCKS))
        // Non-invited players cannot speak in spectatable areas
        return l_invalid;

    QList<QVariant> l_incoming_args;
    for (const QString &l_arg : m_content) {
        l_incoming_args.append(QVariant(l_arg));
    }

    // desk modifier
    QStringList allowed_desk_mods;
    allowed_desk_mods << "chat"
                      << "0"
                      << "1"
                      << "2"
                      << "3"
                      << "4"
                      << "5";
    QString l_incoming_deskmod = l_incoming_args[0].toString();
    if (allowed_desk_mods.contains(l_incoming_deskmod)) {
        // **WARNING : THIS IS A HACK!**
        // A proper solution would be to deprecate chat as an argument on the clientside
        // instead of overwriting correct netcode behaviour on the serverside.
        if (l_incoming_deskmod == "chat") {
            l_args[MS_DESK_MOD] = "1";
        }
        else {
            l_args[MS_DESK_MOD] = l_incoming_args[MS_DESK_MOD].toString();
        }
    }
    else
        return l_invalid;

    // preanim
    l_args[MS_PRE_EMOTE] = l_incoming_args[MS_PRE_EMOTE].toString();

    // char name
    if (client.character().toLower() != l_incoming_args[MS_CHAR_NAME].toString().toLower()) {
        // Selected char is different from supplied folder name
        // This means the user is INI-swapped
        if (!area->iniswapAllowed()) {
            QStringList l_character_split = l_incoming_args[MS_CHAR_NAME].toString().split("/");
            if (!client.getServer()->getCharacters().contains(l_character_split.at(0), Qt::CaseInsensitive) || l_character_split.contains(".."))
                return l_invalid;
        }
        qDebug() << "INI swap detected from " << client.getIpid();
    }
    client.m_current_iniswap = l_incoming_args[MS_CHAR_NAME].toString();
    l_args[MS_CHAR_NAME] = l_incoming_args[MS_CHAR_NAME].toString();

    // emote
    client.m_emote = l_incoming_args[MS_EMOTE].toString();
    if (client.m_first_person)
        client.m_emote = "";
    l_args[MS_EMOTE] = client.m_emote;

    // message text
    if (l_incoming_args[MS_MESSAGE].toString().size() > ConfigManager::maxCharacters())
        return l_invalid;

    // Doublepost prevention. Has to ignore blankposts and testimony commands.
    QString l_incoming_msg = client.dezalgo(l_incoming_args[MS_MESSAGE].toString().trimmed());
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
    l_args[MS_MESSAGE] = l_incoming_msg;

    // side
    // this is validated clientside so w/e
    QString side = area->side();
    if (side.isEmpty()) {
        side = l_incoming_args[MS_SIDE].toString();
    }
    l_args[MS_SIDE] = side;

    if (client.m_pos != l_incoming_args[5].toString()) {
        client.m_pos = l_incoming_args[5].toString();
        client.m_pos.replace("../", "").replace("..\\", "");
        client.updateEvidenceList(client.getServer()->getAreaById(client.areaId()));
    }

    // sfx name
    l_args[MS_SFX_NAME] = l_incoming_args[MS_SFX_NAME].toString();

    // emote modifier
    int emote_mod = l_incoming_args[MS_EMOTE_MOD].toInt();

    if (emote_mod == 4)
        emote_mod = 6;
    if (emote_mod != 0 && emote_mod != 1 && emote_mod != 2 && emote_mod != 5 && emote_mod != 6)
        return l_invalid;
    l_args[MS_EMOTE_MOD] = QString::number(emote_mod);

    // char id
    if (l_incoming_args[MS_CHAR_ID].toInt() != client.m_char_id)
        return l_invalid;
    l_args[MS_CHAR_ID] = l_incoming_args[MS_CHAR_ID].toString();

    // sfx delay
    l_args[MS_SFX_DELAY] = l_incoming_args[MS_SFX_DELAY].toString();

    // objection modifier
    if (area->isShoutAllowed()) {
        if (l_incoming_args[MS_OBJECTION_MOD].toString().contains("4")) {
            // custom shout includes text metadata
            l_args[MS_OBJECTION_MOD] = l_incoming_args[MS_OBJECTION_MOD].toString();
        }
        else {
            int l_obj_mod = l_incoming_args[MS_OBJECTION_MOD].toInt();
            if ((l_obj_mod < 0) || (l_obj_mod > 4)) {
                return l_invalid;
            }
            l_args[MS_OBJECTION_MOD] = QString::number(l_obj_mod);
        }
    }
    else {
        if (l_incoming_args[MS_OBJECTION_MOD].toString() != "0") {
            client.sendServerMessage("Shouts have been disabled in this area.");
        }
        l_args[MS_OBJECTION_MOD] = QString("0");
    }

    // evidence
    int evi_idx = l_incoming_args[MS_EVIDENCE_ID].toInt();
    if (evi_idx > area->evidence().length())
        return l_invalid;
    l_args[MS_EVIDENCE_ID] = QString::number(evi_idx);

    // flipping
    int l_flip = l_incoming_args[MS_FLIP].toInt();
    if (l_flip != 0 && l_flip != 1)
        return l_invalid;
    client.m_flipping = QString::number(l_flip);
    l_args[MS_FLIP] = client.m_flipping;

    // realization
    int realization = l_incoming_args[MS_REALIZATION].toInt();
    if (realization != 0 && realization != 1)
        return l_invalid;
    l_args[MS_REALIZATION] = QString::number(realization);

    // text color
    int text_color = l_incoming_args[MS_TEXT_COLOR].toInt();
    if (text_color < 0 || text_color > 11)
        return l_invalid;
    l_args[MS_TEXT_COLOR] = QString::number(text_color);

    // showname
    QString l_incoming_showname = client.dezalgo(l_incoming_args[MS_SHOWNAME].toString().trimmed());
    if (!(l_incoming_showname == client.character() || l_incoming_showname.isEmpty()) && !area->shownameAllowed()) {
        client.sendServerMessage("Shownames are not allowed in this area!");
        return l_invalid;
    }
    if (l_incoming_showname.length() > 30) {
        client.sendServerMessage("Your showname is too long! Please limit it to under 30 characters");
        return l_invalid;
    }

    // if the raw input is not empty but the trimmed input is, use a single space
    if (l_incoming_showname.isEmpty() && !l_incoming_args[MS_SHOWNAME].toString().isEmpty())
        l_incoming_showname = " ";
    l_args[MS_SHOWNAME] = l_incoming_showname;
    client.setCharacterName(l_incoming_showname);

    // other char id
    QStringList l_pair_data = l_incoming_args[MS_OTHER_CHARID].toString().split("^");
    client.m_pairing_with = l_pair_data[0].toInt();
    QString l_front_back = "";
    if (l_pair_data.length() > 1)
        l_front_back = "^" + l_pair_data[1];
    int l_other_charid = client.m_pairing_with;
    bool l_pairing = false;
    QString l_other_name = "0";
    QString l_other_emote = "0";
    QString l_other_offset = "0";
    QString l_other_flip = "0";
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
    l_args[MS_OTHER_CHARID] = (QString::number(l_other_charid) + l_front_back);
    l_args[MS_OTHER_NAME] = (l_other_name);
    l_args[MS_OTHER_EMOTE] = (l_other_emote);

    // self offset
    client.m_offset = l_incoming_args[MS_SELF_OFFSET].toString();
    l_args[MS_SELF_OFFSET] = client.m_offset;
    l_args[MS_OTHER_OFFSET] = l_other_offset;
    l_args[MS_OTHER_FLIP] = l_other_flip;

    // immediate text processing
    int l_immediate = l_incoming_args[MS_IMMEDIATE].toInt();
    if (area->forceImmediate()) {
        if (l_args[MS_EMOTE_MOD] == "1" || l_args[MS_EMOTE_MOD] == "2") {
            l_args[MS_EMOTE_MOD] = "0";
            l_immediate = 1;
        }
        else if (l_args[MS_EMOTE_MOD] == "6") {
            l_args[MS_EMOTE_MOD] = "5";
            l_immediate = 1;
        }
    }
    if (l_immediate != 1 && l_immediate != 0)
        return l_invalid;
    l_args[MS_IMMEDIATE] = QString::number(l_immediate);

    // sfx looping
    int l_sfx_loop = l_incoming_args[MS_LOOPING_SFX].toInt();
    if (l_sfx_loop != 0 && l_sfx_loop != 1)
        return l_invalid;
    l_args[MS_LOOPING_SFX] = QString::number(l_sfx_loop);

    // screenshake
    int l_screenshake = l_incoming_args[MS_SCREENSHAKE].toInt();
    if (l_screenshake != 0 && l_screenshake != 1)
        return l_invalid;
    l_args[MS_SCREENSHAKE] = QString::number(l_screenshake);

    // frames shake
    l_args[MS_FRAME_SCREENSHAKE] = l_incoming_args[MS_FRAME_SCREENSHAKE].toString();

    // frames realization
    l_args[MS_FRAME_REALIZATION] = l_incoming_args[MS_FRAME_REALIZATION].toString();

    // frames sfx
    l_args[MS_FRAME_SFX] = l_incoming_args[MS_FRAME_SFX].toString();

    // additive
    int l_additive = l_incoming_args[MS_ADDITIVE].toInt();
    if (l_additive != 0 && l_additive != 1)
        return l_invalid;
    else if (area->lastICMessage().isEmpty()) {
        l_additive = 0;
    }
    else if (!(client.m_char_id == area->lastICMessage()[8].toInt())) {
        l_additive = 0;
    }
    else if (l_additive == 1) {
        l_args[MS_MESSAGE].insert(0, " ");
    }
    l_args[MS_ADDITIVE] = QString::number(l_additive);

    // effect
    l_args[MS_EFFECTS] = l_incoming_args[MS_EFFECTS].toString();

    // blips
    l_args[MS_BLIPNAME] = l_incoming_args[MS_BLIPNAME].toString();

    // slide toggle
    l_args[MS_SLIDE] = l_incoming_args[MS_SLIDE].toString();

    // Testimony playback
    QString client_name = client.name();
    if (client_name == "") {
        client_name = client.character(); // fallback in case of empty ooc name
    }
    if (area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING || area->testimonyRecording() == AreaData::TestimonyRecording::ADD) {
        if (!l_args[MS_SIDE].startsWith("wit"))
            return PacketFactory::createPacket("MS", l_args);

        if (area->statement() == -1) {
            l_args[MS_MESSAGE] = "~~-- " + l_args[MS_MESSAGE] + " --";
            l_args[MS_TEXT_COLOR] = "3";
            client.getServer()->broadcast(PacketFactory::createPacket("RT", {"testimony1", "0"}), client.areaId());
        }
        client.addStatement(l_args);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::UPDATE) {
        l_args = client.updateStatement(l_args);
    }
    else if (area->testimonyRecording() == AreaData::TestimonyRecording::PLAYBACK) {
        AreaData::TestimonyProgress l_progress;

        if (l_args[MS_MESSAGE] == ">") {
            auto l_statement = area->jumpToStatement(area->statement() + 1);
            l_args = l_statement.first;
            l_progress = l_statement.second;
            client.m_pos = l_args[MS_SIDE];

            client.sendServerMessageArea(client_name + " moved to the next statement.");

            if (l_progress == AreaData::TestimonyProgress::LOOPED) {
                client.sendServerMessageArea("Last statement reached. Looping to first statement.");
            }
        }
        if (l_args[MS_MESSAGE] == "<") {
            auto l_statement = area->jumpToStatement(area->statement() - 1);
            l_args = l_statement.first;
            l_progress = l_statement.second;
            client.m_pos = l_args[MS_SIDE];

            client.sendServerMessageArea(client_name + " moved to the previous statement.");

            if (l_progress == AreaData::TestimonyProgress::STAYED_AT_FIRST) {
                client.sendServerMessage("First statement reached.");
            }
        }
        QRegularExpressionMatch match = isTestimonyJumpCommand(client.decodeMessage(l_args[MS_MESSAGE])); // Get rid of that pesky encoding, then do the fun part
        if (match.hasMatch()) {
            client.m_pos = "wit";
            int jump_idx = match.captured("int").toInt();
            auto l_statement = area->jumpToStatement(jump_idx);
            l_args = l_statement.first;
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

    return PacketFactory::createPacket("MS", l_args);
}

QRegularExpressionMatch PacketMS::isTestimonyJumpCommand(QString message) const
{
    QRegularExpression jump("(?<arrow>>)(?<int>[0,1,2,3,4,5,6,7,8,9]+)");
    return jump.match(message);
}
