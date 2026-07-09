#include "proto/moderation.h"

#include "akashi/permissions.h"
#include "proto/ao2_protocol.h"
#include "proto/moderation_messages.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

namespace akashi {

namespace {

class ModcallCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<ModcallMessage>();
        l_message->reason = f_packet.field(0);
        l_message->target_id = f_packet.field(1).toInt();
        return l_message;
    }
};

class ModActionCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<ModActionMessage>();
        l_message->target_id = f_packet.field(0).toInt();
        l_message->duration = qMax(f_packet.field(1).toInt(), -1);
        l_message->reason = f_packet.field(2);
        return l_message;
    }
};

class AuthCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<AuthMessage>();
        l_message->system_id = f_packet.field(0);
        for (int i = 1; i < f_packet.fieldCount(); i++) {
            l_message->args.append(f_packet.field(i));
        }
        return l_message;
    }
};

class ModcallHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_modcall = static_cast<const ModcallMessage &>(f_message);

        QString l_caller = f_context.oocName();
        if (l_caller.isEmpty()) {
            l_caller = f_context.character();
        }

        QString l_notice = "!!!MODCALL!!!\nArea: " + f_context.areaName() +
                           "\nCaller: [" + QString::number(f_context.clientId()) + "]" + l_caller + "\n";
        QString l_webhook_reason = l_modcall.reason;
        if (l_modcall.target_id != -1) {
            if (const auto l_target = f_context.playerName(l_modcall.target_id)) {
                l_notice.append("Regarding: " + *l_target + "\n");
                l_webhook_reason.append(" (Regarding: " + *l_target + ")");
            }
        }
        l_notice.append("Reason: " + l_modcall.reason);

        f_context.broadcastModerators(Packet(ao2::HEADER_ZZ, {l_notice}));
        f_context.recordModcall(l_webhook_reason);
    }
};

class ModActionHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_action = static_cast<const ModActionMessage &>(f_message);

        if (!f_context.isAuthenticated()) {
            f_context.sendServerMessage("You are not logged in!");
            return;
        }

        const bool l_is_kick = l_action.duration == 0;
        if (l_is_kick && !f_context.canPerform(permission::kick)) {
            f_context.sendServerMessage("You do not have permission to kick users.");
            return;
        }
        if (!l_is_kick && !f_context.canPerform(permission::ban)) {
            f_context.sendServerMessage("You do not have permission to ban users.");
            return;
        }

        if (!f_context.playerName(l_action.target_id)) {
            f_context.sendServerMessage("User not found.");
            return;
        }

        if (l_is_kick) {
            f_context.kickPlayer(l_action.target_id, l_action.reason);
        }
        else {
            f_context.banPlayer(l_action.target_id, l_action.duration, l_action.reason);
        }
    }
};

// AUTH: a self-describing login. Field 0 must name the server's single
// active system - the honest desync error keeps a client from feeding
// its credentials to a system that is not running.
class AuthenticateHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_auth = static_cast<const AuthMessage &>(f_message);
        if (l_auth.system_id != f_context.activeAuthSystemId()) {
            f_context.sendPacket(Packet(ao2::HEADER_AUTH, {QStringLiteral("0")}));
            f_context.sendServerMessage("This server uses " + f_context.activeAuthSystemId() + " authentication.");
            return;
        }

        f_context.authenticate(l_auth.args);
    }
};

} // namespace

void registerModerationPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs)
{
    const QString l_owner = QStringLiteral("core");

    f_handlers.registerHandler({ao2::HEADER_ZZ, 2, permission::user}, std::make_shared<ModcallHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_MA, 3, permission::user}, std::make_shared<ModActionHandler>(), l_owner);
    // The system id plus at least one argument; joined-only, so a pre-join
    // AUTH dies at dispatch like the rest of the family.
    f_handlers.registerHandler({ao2::HEADER_AUTH, 2, permission::user}, std::make_shared<AuthenticateHandler>(), l_owner);

    f_codecs.registerCodec(ao2::HEADER_ZZ, always(), 0, std::make_shared<ModcallCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_MA, always(), 0, std::make_shared<ModActionCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_AUTH, always(), 0, std::make_shared<AuthCodec>(), l_owner);
}

} // namespace akashi
