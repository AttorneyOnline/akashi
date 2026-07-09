#include "proto/handshake.h"

#include "akashi/area_rule.h"
#include "akashi/permissions.h"
#include "proto/ao2_protocol.h"
#include "proto/handshake_messages.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"
#include "proto/timer_packets.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace akashi {

namespace {

// The empty message for packets that carry no data the server reads.
class EmptyCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        Q_UNUSED(f_packet)
        return std::make_unique<Message>();
    }
};

class HelloCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<HelloMessage>();
        l_message->hwid = f_packet.field(0);
        return l_message;
    }
};

class IdentifyCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<IdentifyMessage>();
        l_message->arch = f_packet.field(0);

        // Matches a version like 2.9.0 anywhere in the version field.
        static const QRegularExpression l_pattern("\\b(\\d+)\\.(\\d+)\\.(\\d+)\\b");
        const QRegularExpressionMatch l_match = l_pattern.match(f_packet.field(1));
        if (l_match.hasMatch()) {
            l_message->version.release = l_match.captured(1).toInt();
            l_message->version.major = l_match.captured(2).toInt();
            l_message->version.minor = l_match.captured(3).toInt();
            l_message->version_valid = true;
        }
        return l_message;
    }
};

class CharacterSelectCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<CharacterSelectMessage>();
        bool l_ok;
        l_message->char_id = f_packet.field(1).toInt(&l_ok);
        if (!l_ok) {
            l_message->char_id = -1;
        }
        return l_message;
    }
};

class HelloHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_hello = static_cast<const HelloMessage &>(f_message);
        if (l_hello.hwid.isEmpty() || !f_context.hwid().isEmpty()) {
            // No double sending or empty HWIDs!
            f_context.sendPacket(Packet(ao2::HEADER_BD, {"A protocol error has been encountered. Packet : HI"}));
            f_context.closeConnection();
            return;
        }

        f_context.setHwid(l_hello.hwid);
        f_context.logConnectionAttempt();
        if (const auto l_ban = f_context.hardwareBan()) {
            const QString l_until = l_ban->permanent ? QStringLiteral("Permanently.")
                                                     : l_ban->end.toString("MM/dd/yyyy, hh:mm");
            f_context.sendPacket(Packet(ao2::HEADER_BD, {"Reason: " + l_ban->reason + "\nBan ID: " + QString::number(l_ban->id) + "\nUntil: " + l_until}));
            f_context.closeConnection();
            return;
        }

        f_context.sendPacket(Packet(ao2::HEADER_ID, {QString::number(f_context.clientId()), "akashi", QCoreApplication::applicationVersion()}));
    }
};

class IdentifyHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_identify = static_cast<const IdentifyMessage &>(f_message);
        if (f_context.isIdentified()) {
            // No double sending of the ID packet!
            f_context.sendPacket(Packet(ao2::HEADER_BD, {"A protocol error has been encountered. Packet : ID"}));
            f_context.closeConnection();
            return;
        }

        if (!f_context.webaoEnabled() && l_identify.arch == "webAO") {
            f_context.sendPacket(Packet(ao2::HEADER_BD, {"WebAO is disabled on this server."}));
            f_context.closeConnection();
            return;
        }

        // Any release that parses is accepted (AO2 is release 2, DRO reports
        // release 1, etc.); only a version field with no X.Y.Z is a protocol error.
        if (!l_identify.version_valid) {
            f_context.sendPacket(Packet(ao2::HEADER_BD, {"A protocol error has been encountered. Packet : ID\nVersion not recognised."}));
            f_context.closeConnection();
            return;
        }

        ClientProfile l_profile;
        l_profile.arch = l_identify.arch;
        l_profile.version = l_identify.version;
        f_context.identify(l_profile);

        f_context.sendPacket(Packet(ao2::HEADER_PN, {QString::number(f_context.playerCount()), QString::number(f_context.maxPlayerCount()), f_context.serverDescription()}));

        f_context.sendPacket(Packet(ao2::HEADER_FL, serverFeatures()));

        if (f_context.assetUrl().isValid()) {
            const QByteArray l_asset_url = f_context.assetUrl().toEncoded(QUrl::EncodeSpaces);
            f_context.sendPacket(Packet(ao2::HEADER_ASS, {QString::fromUtf8(l_asset_url)}));
        }
    }
};

class ResourceCountHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        Q_UNUSED(f_message)
        // Evidence is not loaded during this part anymore, so the count is
        // always zero. The client only cares about what it gets from LE.
        f_context.sendPacket(Packet(ao2::HEADER_SI, {QString::number(f_context.characters().size()), "0", QString::number(f_context.floorAreaNames().size() + f_context.musicList().size())}));
    }
};

class CharacterListHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        Q_UNUSED(f_message)
        f_context.sendPacket(Packet(ao2::HEADER_SC, f_context.characters()));
    }
};

class MusicListHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        Q_UNUSED(f_message)
        f_context.sendPacket(Packet(ao2::HEADER_SM, f_context.floorAreaNames() + f_context.musicList()));
    }
};

class JoinHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        Q_UNUSED(f_message)
        if (f_context.hwid().isEmpty()) {
            // No early connecting!
            f_context.closeConnection();
            return;
        }

        if (f_context.isJoined()) {
            return;
        }

        // A blocking server_joined rule turns the connection away like a
        // ban does: reason shown, never counted as joined.
        if (auto l_block = f_context.checkBeforeRule(AreaEvents::ServerJoined, {})) {
            f_context.sendPacket(Packet(ao2::HEADER_BD, {*l_block}));
            f_context.closeConnection();
            return;
        }

        f_context.markJoined();
        f_context.announceCharsTaken();

        // The area's state (evidence, penalties, background, timers, music,
        // floor map) reaches the client through the player_joined after-rules.
        f_context.runAfterRule(AreaEvents::PlayerJoined, {{QStringLiteral("from_area"), -1}, {QStringLiteral("from_floor"), -1}});

        sendTimer(f_context, 0, f_context.globalTimer());
        // Here lies OPPASS, the genius of FanatSors who send the modpass to everyone in plain text.
        f_context.sendPacket(Packet(ao2::HEADER_DONE));

        f_context.sendServerMessage("=== MOTD ===\r\n" + f_context.motd() + "\r\n=============");

        // Once per session, unlike player_joined which fires on every area entry.
        f_context.runAfterRule(AreaEvents::ServerJoined, {});

        f_context.finishJoin();
        // Tell everyone there is a new player.
        f_context.broadcastPlayerCount();
    }

  private:
    static void sendTimer(IPacketContext &f_context, int f_timer_id, const TimerSnapshot &f_timer)
    {
        if (f_timer.running) {
            f_context.sendPacket(timerShow(f_timer_id));
            f_context.sendPacket(timerValue(f_timer_id, f_timer.remaining_ms));
        }
        else {
            f_context.sendPacket(timerHide(f_timer_id));
        }
    }
};

class CharacterSelectHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_select = static_cast<const CharacterSelectMessage &>(f_message);
        if (l_select.char_id < -1 || l_select.char_id > f_context.characters().size() - 1) {
            // The connection closes, but the selection below still runs, like it always has.
            f_context.sendPacket(Packet(ao2::HEADER_KK, {"A protocol error has been encountered.Packet : CC\nCharacter ID out of range."}));
            f_context.closeConnection();
        }

        f_context.selectCharacter(l_select.char_id);
    }
};

// CH: the keepalive; the client measures its ping on the answer.
class KeepaliveHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        Q_UNUSED(f_message)
        f_context.sendPacket(Packet(ao2::HEADER_CHECK));
    }
};

class CharacterPasswordCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<CharacterPasswordMessage>();
        l_message->password = f_packet.field(0);
        return l_message;
    }
};

class CharacterPasswordHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_password = static_cast<const CharacterPasswordMessage &>(f_message);
        f_context.setCharacterPassword(l_password.password);
    }
};

} // namespace

QStringList serverFeatures()
{
    return {
        "noencryption", "yellowtext", "prezoom",
        "flipping", "customobjections", "fastloading",
        "deskmod", "evidence", "cccc_ic_support",
        "arup", "casing_alerts", "modcall_reason",
        "looping_sfx", "additive", "effects",
        "y_offset", "expanded_desk_mods", "auth_packet", "custom_blips"};
}

void registerHandshakePackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs)
{
    const QString l_owner = QStringLiteral("core");

    // The handshake surface is the only one open to a fresh connection;
    // everything past RD requires the user tier.
    f_handlers.registerHandler({ao2::HEADER_HI, 1, permission::none}, std::make_shared<HelloHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_ID, 2, permission::none}, std::make_shared<IdentifyHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_ASKCHAA, 0, permission::none}, std::make_shared<ResourceCountHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_RC, 0, permission::none}, std::make_shared<CharacterListHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_RM, 0, permission::none}, std::make_shared<MusicListHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_RD, 0, permission::none}, std::make_shared<JoinHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_CC, 3, permission::user}, std::make_shared<CharacterSelectHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_CH, 1, permission::none}, std::make_shared<KeepaliveHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_PW, 1, permission::user}, std::make_shared<CharacterPasswordHandler>(), l_owner);

    f_codecs.registerCodec(ao2::HEADER_HI, always(), 0, std::make_shared<HelloCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_ID, always(), 0, std::make_shared<IdentifyCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_CC, always(), 0, std::make_shared<CharacterSelectCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_PW, always(), 0, std::make_shared<CharacterPasswordCodec>(), l_owner);
    // Every other packet decodes to the empty message until its family moves over.
    f_codecs.registerCodec(QStringLiteral("*"), always(), 0, std::make_shared<EmptyCodec>(), l_owner);
}

} // namespace akashi
