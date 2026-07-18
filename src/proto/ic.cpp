#include "proto/ic.h"

#include "akashi/area_rule.h"
#include "akashi/permissions.h"
#include "proto/ao2_protocol.h"
#include "proto/packet_registry.h"
#include "proto/text_utils.h"

#include <QRegularExpression>

namespace akashi {

namespace {

// The testimony playback jump command, like >3 or <2.
QRegularExpressionMatch testimonyJumpCommand(const QString &f_message)
{
    static const QRegularExpression l_jump("(?<arrow>>|<)(?<int>\\d+)");
    return l_jump.match(f_message);
}

// Splits the protocol's pair field, like "4^1", into id and tag.
void readPairField(const QString &f_field, ICMessage &f_message)
{
    const QStringList l_parts = f_field.split("^");
    f_message.pair_requested = l_parts[0].toInt();
    if (l_parts.size() > 1) {
        f_message.pair_front_back = "^" + l_parts[1];
    }
}

} // namespace

std::unique_ptr<Message> Ao2IcCodec::decode(const Packet &f_packet) const
{
    const int l_count = f_packet.fieldCount();
    // The message needs its full base field set.
    if (l_count < 15) {
        return nullptr;
    }
    auto l_message = std::make_unique<ICMessage>();
    ICMessage &l_ic = *l_message;

    l_ic.desk_mod = f_packet.field(0);
    l_ic.preanim = f_packet.field(1);
    l_ic.char_name = f_packet.field(2);
    l_ic.emote = f_packet.field(3);
    l_ic.message_text = f_packet.field(4);
    l_ic.side = f_packet.field(5);
    l_ic.sfx_name = f_packet.field(6);
    l_ic.emote_mod = f_packet.field(7).toInt();
    l_ic.char_id = f_packet.field(8).toInt();
    l_ic.sfx_delay = f_packet.field(9).toInt();
    l_ic.objection_mod = f_packet.field(10);
    l_ic.evidence = f_packet.field(11).toInt();
    l_ic.flip = f_packet.field(12).toInt();
    l_ic.realization = f_packet.field(13).toInt();
    l_ic.text_color = f_packet.field(14).toInt();

    if (l_count >= 19) {
        l_ic.has_pair_data = true;
        l_ic.showname = f_packet.field(15);
        readPairField(f_packet.field(16), l_ic);
        l_ic.self_offset = f_packet.field(17);
        l_ic.immediate = f_packet.field(18).toInt();
    }
    if (l_count >= 26) {
        l_ic.has_effect_data = true;
        l_ic.sfx_looping = f_packet.field(19).toInt();
        l_ic.screenshake = f_packet.field(20).toInt();
        l_ic.frames_shake = f_packet.field(21);
        l_ic.frames_realization = f_packet.field(22);
        l_ic.frames_sfx = f_packet.field(23);
        l_ic.additive = f_packet.field(24).toInt();
        l_ic.effect = f_packet.field(25);
    }
    if (l_count >= 27) {
        l_ic.has_blips = true;
        l_ic.blips = f_packet.field(26);
    }
    if (l_count >= 28) {
        l_ic.has_slide = true;
        l_ic.slide = f_packet.field(27);
    }
    return l_message;
}

Packet Ao2IcCodec::encode(const Message &f_message) const
{
    const auto &l_ic = static_cast<const ICMessage &>(f_message);
    QStringList l_fields = {
        l_ic.desk_mod, l_ic.preanim, l_ic.char_name, l_ic.emote, l_ic.message_text,
        l_ic.side, l_ic.sfx_name, QString::number(l_ic.emote_mod), QString::number(l_ic.char_id),
        QString::number(l_ic.sfx_delay), l_ic.objection_mod, QString::number(l_ic.evidence),
        QString::number(l_ic.flip), QString::number(l_ic.realization), QString::number(l_ic.text_color)};

    if (l_ic.has_pair_data) {
        l_fields << l_ic.showname
                 << QString::number(l_ic.other_char_id) + l_ic.pair_front_back
                 << l_ic.other_name << l_ic.other_emote
                 << l_ic.self_offset << l_ic.other_offset << l_ic.other_flip
                 << QString::number(l_ic.immediate);
    }
    if (l_ic.has_effect_data) {
        l_fields << QString::number(l_ic.sfx_looping) << QString::number(l_ic.screenshake)
                 << l_ic.frames_shake << l_ic.frames_realization << l_ic.frames_sfx
                 << QString::number(l_ic.additive) << l_ic.effect;
    }
    if (l_ic.has_blips) {
        l_fields << l_ic.blips;
    }
    if (l_ic.has_slide) {
        l_fields << l_ic.slide;
    }
    return Packet(ao2::HEADER_MS, l_fields);
}

namespace {

class IcHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        ICMessage l_ic = static_cast<const ICMessage &>(f_message);
        if (!f_context.canUseIcChat()) {
            f_context.sendServerMessage("You cannot speak while muted.");
            return;
        }
        if (!f_context.isIcMessageAllowed()) {
            return;
        }

        // Validation, in the exact order the old validateIcPacket used.
        if (f_context.isSpectator() || f_context.character().isEmpty()) {
            // Spectators cannot use IC.
            return;
        }
        if (!f_context.canActInArea()) {
            // Non-invited players cannot speak in spectatable areas.
            return;
        }

        const auto l_rule_block = f_context.checkBeforeRule(AreaEvents::IcMessageSent,
                                                            {{QStringLiteral("message"), l_ic.message_text},
                                                             {QStringLiteral("char_name"), l_ic.char_name},
                                                             {QStringLiteral("objection_mod"), l_ic.objection_mod},
                                                             {QStringLiteral("showname"), l_ic.showname},
                                                             {QStringLiteral("evidence"), l_ic.evidence}});
        if (l_rule_block) {
            f_context.sendServerMessage(*l_rule_block);
            return;
        }

        // Desk modifier; "chat" is an old client quirk meaning shown.
        static const QStringList l_allowed_desk_mods = {"chat", "0", "1", "2", "3", "4", "5"};
        if (!l_allowed_desk_mods.contains(l_ic.desk_mod)) {
            return;
        }
        if (l_ic.desk_mod == "chat") {
            l_ic.desk_mod = "1";
        }

        // Emote; first-person users show none to others. The iniswap and
        // emote commits wait for the commit block below.
        if (f_context.isFirstPerson()) {
            l_ic.emote = "";
        }

        // Message text.
        if (l_ic.message_text.size() > f_context.maxMessageLength()) {
            return;
        }
        QString l_text = stripZalgo(l_ic.message_text.trimmed());
        const bool l_is_testimony_command = testimonyJumpCommand(Packet::unescape(l_text)).hasMatch() || l_text == ">" || l_text == "<";
        if (!f_context.lastIcMessage().isEmpty() && l_text == f_context.lastIcMessage() && !l_is_testimony_command) {
            // No doubleposting. The memory itself only commits after every
            // check passed, so a refused message never swallows its own
            // corrected resend.
            return;
        }

        auto l_filtered = f_context.applyTextFilters(l_text, TextChannel::Ic);
        if (!l_filtered)
            return;
        l_ic.message_text = *l_filtered;

        // Side; the area's own side wins when it has one. The position
        // commit waits until the whole message validated.
        const QString l_requested_side = l_ic.side;
        const QString l_area_side = f_context.areaSide();
        if (!l_area_side.isEmpty()) {
            l_ic.side = l_area_side;
        }

        // Emote modifier. A 4 crashes clients, so it has always been bent to 6.
        if (l_ic.emote_mod == 4) {
            l_ic.emote_mod = 6;
        }
        if (l_ic.emote_mod != 0 && l_ic.emote_mod != 1 && l_ic.emote_mod != 2 && l_ic.emote_mod != 5 && l_ic.emote_mod != 6) {
            return;
        }

        // The sender can only speak as its own character.
        if (l_ic.char_id != f_context.characterId()) {
            return;
        }

        // Objection modifier hygiene. A custom shout is "4&" plus its display
        // text; everything else must be a plain 0-4. The old substring test
        // (contains "4") let any field with a 4 anywhere in it skip
        // validation and broadcast verbatim.
        if (l_ic.objection_mod.startsWith(QStringLiteral("4&"))) {
            // The text after the separator is short display metadata; cap it
            // so the field cannot smuggle an arbitrary payload to the room.
            constexpr int max_custom_shout = 256;
            if (l_ic.objection_mod.size() > 2 + max_custom_shout) {
                l_ic.objection_mod = l_ic.objection_mod.left(2 + max_custom_shout);
            }
        }
        else {
            bool l_objection_ok = false;
            const int l_objection = l_ic.objection_mod.toInt(&l_objection_ok);
            if (!l_objection_ok || l_objection < 0 || l_objection > 4) {
                return;
            }
            l_ic.objection_mod = QString::number(l_objection);
        }

        // The transform rules rewrite the payload between gate and commit:
        // strip_shouts downgrades the shout, apply_medieval reworks the
        // text. bypass_rules does not skip them - transforms are area
        // flavor, not gates, so a moderator's message is transformed too.
        const QVariantMap l_transformed = f_context.runTransformRules(AreaEvents::IcMessageSent,
                                                                      {{QStringLiteral("message"), l_ic.message_text},
                                                                       {QStringLiteral("char_name"), l_ic.char_name},
                                                                       {QStringLiteral("objection_mod"), l_ic.objection_mod},
                                                                       {QStringLiteral("showname"), l_ic.showname},
                                                                       {QStringLiteral("evidence"), l_ic.evidence}});
        l_ic.message_text = l_transformed.value(QStringLiteral("message")).toString();
        l_ic.objection_mod = l_transformed.value(QStringLiteral("objection_mod")).toString();

        // Evidence.
        if (l_ic.evidence > f_context.evidenceCount()) {
            return;
        }
        // Blocking the event refuses the presentation, not the message.
        if (l_ic.evidence > 0) {
            if (const auto l_evidence_block = f_context.checkBeforeRule(AreaEvents::EvidencePresented, {{QStringLiteral("index"), l_ic.evidence}})) {
                l_ic.evidence = 0;
                f_context.sendServerMessage(*l_evidence_block);
            }
        }

        // Flipping, realization and text color.
        if (l_ic.flip != 0 && l_ic.flip != 1) {
            return;
        }
        if (l_ic.realization != 0 && l_ic.realization != 1) {
            return;
        }
        if (l_ic.text_color < 0 || l_ic.text_color > 11) {
            return;
        }

        if (l_ic.has_pair_data) {
            if (!validatePairData(l_ic, f_context)) {
                return;
            }
        }
        if (l_ic.has_effect_data) {
            if (!validateEffectData(l_ic, f_context)) {
                return;
            }
        }

        // Nothing refuses the message past this point, so only now does it
        // touch session state: a refused message leaves the doublepost
        // memory, position, iniswap, emote, flip, showname, pair offset and
        // pair request exactly as they were.
        f_context.setLastIcMessage(l_text);
        // A character-name mismatch means an iniswap; the check_iniswap
        // floor rule gated it at the top.
        f_context.setIniswap(l_ic.char_name);
        f_context.setEmote(l_ic.emote);
        f_context.setFlipping(QString::number(l_ic.flip));
        f_context.updatePosition(l_requested_side);
        if (l_ic.has_pair_data) {
            f_context.setCharacterName(l_ic.showname);
            // The full offset; old clients only get a trimmed one on the
            // wire, in resolvePairFields below.
            f_context.setOffset(l_ic.self_offset);
            f_context.setPairingWith(l_ic.pair_requested);
        }

        // Pair resolution is enrichment, not validation: it reads the
        // committed state, so a same-packet side change pairs at the new
        // position, and it cannot refuse the message.
        if (l_ic.has_pair_data) {
            resolvePairFields(l_ic, f_context);
        }

        // Serialize, run the testimony recorder over the result, and send it out.
        QStringList l_fields = Ao2IcCodec().encode(l_ic).fields();
        l_fields = f_context.applyTestimony(l_fields);
        f_context.broadcastIc(l_fields, l_ic.evidence);
        f_context.runAfterRule(AreaEvents::IcMessageSent, {{QStringLiteral("message"), l_ic.message_text}});
        if (l_ic.evidence > 0) {
            f_context.runAfterRule(AreaEvents::EvidencePresented, {{QStringLiteral("index"), l_ic.evidence}});
        }
    }

  private:
    // Checks and rewrites the pair block on the message only; the session
    // commits (showname, pair offset, pair request) stay with the caller
    // until every validation passed, so a refused message leaves no state
    // behind. The partner lookup lives in resolvePairFields.
    bool validatePairData(ICMessage &f_ic, IPacketContext &f_context) const
    {
        // Showname. The area's showname policy is the check_showname
        // before-rule; only length and whitespace hygiene stay here, and
        // the length check runs before anything else.
        QString l_showname = stripZalgo(f_ic.showname.trimmed());
        if (l_showname.size() > 30) {
            f_context.sendServerMessage("Your showname is too long! Please limit it to under 30 characters");
            return false;
        }
        // A showname of only whitespace stays visible as a single space.
        if (l_showname.isEmpty() && !f_ic.showname.isEmpty()) {
            l_showname = " ";
        }
        f_ic.showname = l_showname;

        // Immediate text, with the area's forced setting applied.
        if (f_context.isImmediateForced()) {
            if (f_ic.emote_mod == 1 || f_ic.emote_mod == 2) {
                f_ic.emote_mod = 0;
                f_ic.immediate = 1;
            }
            else if (f_ic.emote_mod == 6) {
                f_ic.emote_mod = 5;
                f_ic.immediate = 1;
            }
        }
        if (f_ic.immediate != 1 && f_ic.immediate != 0) {
            return false;
        }
        return true;
    }

    // Fills the partner's fields into the message after the commit block.
    // Pairing only works when both sides chose each other on the same spot.
    void resolvePairFields(ICMessage &f_ic, IPacketContext &f_context) const
    {
        const PairInfo l_pair = f_context.resolvePair(f_ic.pair_requested);
        f_ic.other_char_id = l_pair.paired ? f_ic.pair_requested : -1;
        if (!l_pair.paired) {
            f_ic.pair_front_back = "";
        }
        f_ic.other_name = l_pair.name;
        f_ic.other_emote = l_pair.emote;
        f_ic.other_flip = l_pair.flip;

        // Versions 2.6 through 2.8 cannot handle a y offset, so they get x alone.
        const ClientVersion l_version = f_context.profile().version;
        if (l_version.release == 2 && (l_version.major == 6 || l_version.major == 7 || l_version.major == 8)) {
            f_ic.self_offset = f_ic.self_offset.split("&")[0];
            f_ic.other_offset = l_pair.offset.split("&")[0];
        }
        else {
            f_ic.other_offset = l_pair.offset;
        }
    }

    bool validateEffectData(ICMessage &f_ic, IPacketContext &f_context) const
    {
        if (f_ic.sfx_looping != 0 && f_ic.sfx_looping != 1) {
            return false;
        }
        if (f_ic.screenshake != 0 && f_ic.screenshake != 1) {
            return false;
        }

        // Additive continues the sender's own previous message.
        if (f_ic.additive != 0 && f_ic.additive != 1) {
            return false;
        }
        const QStringList l_last = f_context.lastAreaMessage();
        if (l_last.isEmpty()) {
            f_ic.additive = 0;
        }
        else if (f_ic.char_id != l_last[8].toInt()) {
            f_ic.additive = 0;
        }
        else if (f_ic.additive == 1) {
            f_ic.message_text.insert(0, " ");
        }
        return true;
    }
};

} // namespace

ICMessage icMessageFromOutgoingFields(const QStringList &f_fields)
{
    ICMessage l_ic;
    l_ic.desk_mod = f_fields.value(0);
    l_ic.preanim = f_fields.value(1);
    l_ic.char_name = f_fields.value(2);
    l_ic.emote = f_fields.value(3);
    l_ic.message_text = f_fields.value(4);
    l_ic.side = f_fields.value(5);
    l_ic.sfx_name = f_fields.value(6);
    l_ic.emote_mod = f_fields.value(7).toInt();
    l_ic.char_id = f_fields.value(8).toInt();
    l_ic.sfx_delay = f_fields.value(9).toInt();
    l_ic.objection_mod = f_fields.value(10);
    l_ic.evidence = f_fields.value(11).toInt();
    l_ic.flip = f_fields.value(12).toInt();
    l_ic.realization = f_fields.value(13).toInt();
    l_ic.text_color = f_fields.value(14).toInt();

    if (f_fields.size() >= 23) {
        l_ic.has_pair_data = true;
        l_ic.showname = f_fields.value(15);
        const QStringList l_pair = f_fields.value(16).split("^");
        l_ic.other_char_id = l_pair[0].toInt();
        if (l_pair.size() > 1) {
            l_ic.pair_front_back = "^" + l_pair[1];
        }
        l_ic.other_name = f_fields.value(17);
        l_ic.other_emote = f_fields.value(18);
        l_ic.self_offset = f_fields.value(19);
        l_ic.other_offset = f_fields.value(20);
        l_ic.other_flip = f_fields.value(21);
        l_ic.immediate = f_fields.value(22).toInt();
    }
    if (f_fields.size() >= 30) {
        l_ic.has_effect_data = true;
        l_ic.sfx_looping = f_fields.value(23).toInt();
        l_ic.screenshake = f_fields.value(24).toInt();
        l_ic.frames_shake = f_fields.value(25);
        l_ic.frames_realization = f_fields.value(26);
        l_ic.frames_sfx = f_fields.value(27);
        l_ic.additive = f_fields.value(28).toInt();
        l_ic.effect = f_fields.value(29);
    }
    if (f_fields.size() >= 31) {
        l_ic.has_blips = true;
        l_ic.blips = f_fields.value(30);
    }
    if (f_fields.size() >= 32) {
        l_ic.has_slide = true;
        l_ic.slide = f_fields.value(31);
    }
    return l_ic;
}

void registerIcPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs)
{
    const QString l_owner = QStringLiteral("core");

    // The codec checks the field shape itself, so the spec only needs one field.
    f_handlers.registerHandler({ao2::HEADER_MS, 1, permission::user}, std::make_shared<IcHandler>(), l_owner);

    f_codecs.registerCodec(ao2::HEADER_MS, always(), 0, std::make_shared<Ao2IcCodec>(), l_owner);
}

} // namespace akashi
