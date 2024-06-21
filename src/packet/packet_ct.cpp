#include "packet/packet_ct.h"
#include "akashidefs.h"
#include "config_manager.h"
#include "packet/packet_factory.h"
#include "server.h"

#include <QDebug>

PacketCT::PacketCT(QStringList &contents) :
    AOPacket(contents)
{
}

PacketInfo PacketCT::getPacketInfo() const
{
    PacketInfo info{
        .acl_permission = ACLRole::Permission::NONE,
        .min_args = 2,
        .header = "CT"};
    return info;
}

void PacketCT::handlePacket(AreaData *area, AOClient &client) const
{
    if (client.m_is_ooc_muted) {
        client.sendServerMessage("You are OOC muted, and cannot speak.");
        return;
    }

    client.setName(client.dezalgo(m_content[0]).replace(QRegExp("\\[|\\]|\\{|\\}|\\#|\\$|\\%|\\&"), "")); // no fucky wucky shit here
    if (client.name().isEmpty() || client.name() == ConfigManager::serverName())                          // impersonation & empty name protection
        return;

    if (client.name().length() > 30) {
        client.sendServerMessage("Your name is too long! Please limit it to under 30 characters.");
        return;
    }

    if (client.m_is_logging_in) {
        client.loginAttempt(m_content[1]);
        return;
    }

    QString l_message = client.dezalgo(m_content[1]);
    if (l_message.length() == 0 || l_message.length() > ConfigManager::maxCharacters())
        return;
    AOPacket *final_packet = PacketFactory::createPacket("CT", {client.name(), l_message, "0"});
    if (l_message.at(0) == '/') {
        QStringList l_cmd_argv = l_message.split(" ", akashi::SkipEmptyParts);
        QString l_command = l_cmd_argv[0].trimmed().toLower();
        l_command = l_command.right(l_command.length() - 1);
        l_cmd_argv.removeFirst();
        int l_cmd_argc = l_cmd_argv.length();

        client.handleCommand(l_command, l_cmd_argc, l_cmd_argv);
        emit client.logCMD((client.character() + " " + client.characterName()), client.m_ipid, client.name(), l_command, l_cmd_argv, client.getServer()->getAreaById(client.areaId())->name());
        return;
    }
    else {
        client.getServer()->broadcast(final_packet, client.areaId());
    }
    emit client.logOOC((client.character() + " " + client.characterName()), client.name(), client.m_ipid, area->name(), l_message);
}
