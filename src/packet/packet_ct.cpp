#include "packet/packet_ct.h"

#include "config_manager.h"
#include "packet/packet_factory.h"
#include "server.h"

#include <QDebug>
#include <QRegularExpression>

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

    client.setName(client.dezalgo(m_content[0]).replace(QRegularExpression("\\[|\\]|\\{|\\}|\\#|\\$|\\%|\\&"), "")); // no fucky wucky shit here
    if (client.name().trimmed().replace("​", "").isEmpty() || client.name() == ConfigManager::serverName())        // impersonation & empty name protection
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

    if (!ConfigManager::filterList().isEmpty()) {
        foreach (const QString &regex, ConfigManager::filterList()) {
            QRegularExpression re(regex, QRegularExpression::CaseInsensitiveOption);
            l_message.replace(re, "❌");
        }
    }

    if (l_message.at(0) == '/') {
        QStringList l_cmd_argv = l_message.split(" ", Qt::SkipEmptyParts);
        QString l_command = l_cmd_argv[0].trimmed().toLower();
        l_command = l_command.right(l_command.length() - 1);
        l_cmd_argv.removeFirst();
        int l_cmd_argc = l_cmd_argv.length();

        client.handleCommand(l_command, l_cmd_argc, l_cmd_argv);
        emit client.logCMD((client.character() + " " + client.characterName()), client.m_ipid, client.name(), l_command, l_cmd_argv, client.getServer()->getAreaById(client.areaId())->name());
        return;
    }
    else {
        AOPacket *final_packet = PacketFactory::createPacket("CT", {client.name(), l_message, "0"});
        client.getServer()->broadcast(final_packet, client.areaId());
    }
    emit client.logOOC((client.character() + " " + client.characterName()), client.name(), client.m_ipid, area->name(), l_message);
}
