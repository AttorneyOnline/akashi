#include "proto/area_music.h"

#include "akashi/area_rule.h"
#include "akashi/permissions.h"
#include "proto/ao2_protocol.h"
#include "proto/area_music_messages.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

namespace akashi {

namespace {

class MusicChangeCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<MusicChangeMessage>();
        l_message->argument = f_packet.field(0);
        l_message->char_id = f_packet.field(1);
        if (f_packet.fieldCount() >= 4) {
            l_message->has_effects = true;
            l_message->effects = f_packet.field(3);
        }
        return l_message;
    }
};

class JudgeSplashCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<JudgeSplashMessage>();
        l_message->splash = f_packet.field(0);
        if (f_packet.fieldCount() >= 2) {
            l_message->has_variant = true;
            l_message->variant = f_packet.field(1);
        }
        return l_message;
    }
};

class EvidenceAddCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<EvidenceAddMessage>();
        l_message->name = f_packet.field(0);
        l_message->description = f_packet.field(1);
        l_message->image = f_packet.field(2);
        return l_message;
    }
};

class PenaltyCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<PenaltyMessage>();
        l_message->bar = f_packet.field(0).toInt();
        l_message->value = f_packet.field(1).toInt();
        return l_message;
    }
};

class MusicChangeHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_music = static_cast<const MusicChangeMessage &>(f_message);

        // The packet historically does two jobs: an argument that is no
        // song moves the client to the area of that name instead.
        if (!f_context.hasSong(l_music.argument) && l_music.argument != ao2::STOP_MUSIC) {
            const int l_local = f_context.floorAreaNames().indexOf(l_music.argument);
            if (l_local >= 0) {
                int l_global = f_context.floorAreaToGlobal(l_local);
                if (l_global >= 0)
                    f_context.changeArea(l_global);
            }
            return;
        }

        if (f_context.isSpectator() || !f_context.canActInArea()) {
            f_context.sendServerMessage("Spectators are blocked from changing the music.");
            return;
        }
        if (f_context.isDjBlocked()) {
            f_context.sendServerMessage("You are blocked from changing the music.");
            return;
        }

        // The verb owns everything from the rules on: jukebox interception,
        // alias resolution, the mutation, the broadcast and the music log.
        const auto l_refusal = f_context.playMusic(l_music.argument, MusicSource::List, l_music.char_id,
                                                   l_music.has_effects ? l_music.effects : QStringLiteral("0"));
        if (l_refusal) {
            f_context.sendServerMessage(*l_refusal);
        }
    }
};

class JudgeSplashHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_splash = static_cast<const JudgeSplashMessage &>(f_message);

        if (f_context.isSpectator() || !f_context.canActInArea()) {
            f_context.sendServerMessage("Spectators are blocked from using the judge controls.");
            return;
        }
        if (f_context.isWtceBlocked()) {
            f_context.sendServerMessage("You are blocked from using the judge controls.");
            return;
        }
        // Still cooling down; dropped silently.
        if (!f_context.startWtceCooldown()) {
            return;
        }

        // The verb owns the wtce_used rules, the broadcast and the log.
        const auto l_refusal = f_context.useWtce(l_splash.splash, l_splash.has_variant ? std::optional<QString>(l_splash.variant) : std::nullopt);
        if (l_refusal) {
            f_context.sendServerMessage(*l_refusal);
        }
    }
};

class EvidenceAddHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_evidence = static_cast<const EvidenceAddMessage &>(f_message);

        // Evidence access is enforced by the check_evidence_access floor rule.
        auto l_rule_block = f_context.checkBeforeRule(AreaEvents::EvidenceAdded, {{QStringLiteral("name"), l_evidence.name}});
        if (l_rule_block) {
            f_context.sendServerMessage(*l_rule_block);
            return;
        }

        // The hidden-CM owner tagging happens inside the evidence path.
        f_context.addEvidence(l_evidence.name, l_evidence.description, l_evidence.image);
        f_context.runAfterRule(AreaEvents::EvidenceAdded, {{QStringLiteral("name"), l_evidence.name}});
    }
};

class PenaltyHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_penalty = static_cast<const PenaltyMessage &>(f_message);

        if (f_context.isSpectator() || !f_context.canActInArea()) {
            f_context.sendServerMessage("Spectators are blocked from using the judge controls.");
            return;
        }
        if (f_context.isWtceBlocked()) {
            f_context.sendServerMessage("You are blocked from using the judge controls.");
            return;
        }

        // The verb owns the wtce_used rules, the mutation, both HP
        // broadcasts and the log.
        const auto l_refusal = f_context.changePenalty(l_penalty.bar, l_penalty.value);
        if (l_refusal) {
            f_context.sendServerMessage(*l_refusal);
        }
    }
};

} // namespace

void registerAreaMusicPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs)
{
    const QString l_owner = QStringLiteral("core");

    f_handlers.registerHandler({ao2::HEADER_MC, 2, permission::user}, std::make_shared<MusicChangeHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_RT, 1, permission::user}, std::make_shared<JudgeSplashHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_PE, 3, permission::user}, std::make_shared<EvidenceAddHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_HP, 2, permission::user}, std::make_shared<PenaltyHandler>(), l_owner);

    f_codecs.registerCodec(ao2::HEADER_MC, always(), 0, std::make_shared<MusicChangeCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_RT, always(), 0, std::make_shared<JudgeSplashCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_PE, always(), 0, std::make_shared<EvidenceAddCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_HP, always(), 0, std::make_shared<PenaltyCodec>(), l_owner);
}

} // namespace akashi
