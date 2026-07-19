#include "proto/chat.h"

#include "akashi/area_rule.h"
#include "akashi/permissions.h"
#include "proto/ao2_protocol.h"
#include "proto/chat_messages.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"
#include "proto/text_utils.h"

#include <QRegularExpression>

namespace akashi {

namespace {

class OocCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<OocMessage>();
        l_message->name = f_packet.field(0);
        l_message->message = f_packet.field(1);
        return l_message;
    }
};

class EvidenceDeleteCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<EvidenceDeleteMessage>();
        bool l_ok;
        const int l_index = f_packet.field(0).toInt(&l_ok);
        if (l_ok) {
            l_message->index = l_index;
        }
        return l_message;
    }
};

class EvidenceEditCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<EvidenceEditMessage>();
        bool l_ok;
        const int l_index = f_packet.field(0).toInt(&l_ok);
        if (l_ok) {
            l_message->index = l_index;
        }
        l_message->name = f_packet.field(1);
        l_message->description = f_packet.field(2);
        l_message->image = f_packet.field(3);
        return l_message;
    }
};

class CasingPreferencesCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<CasingPreferencesMessage>();
        QList<bool> l_preferences;
        for (int i = 2; i <= 6; i++) {
            bool l_ok;
            const int l_value = f_packet.field(i).toInt(&l_ok);
            if (!l_ok) {
                return l_message;
            }
            l_preferences.append(l_value);
        }
        l_message->preferences = l_preferences;
        return l_message;
    }
};

class OocHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_ooc = static_cast<const OocMessage &>(f_message);
        // An OOC mute silences the chat, not the doors: the login flow
        // stays open (a sanction must never gate authentication - a
        // stored mute would otherwise brick the account forever), and an
        // authenticated moderator keeps their commands, since a sanction
        // may suppress speech but never strip a role-granted tool.
        if (!f_context.canUseOocChat()) {
            const QString l_line = l_ooc.message.trimmed();
            const QString l_verb = l_line.startsWith(QLatin1Char('/'))
                                       ? l_line.mid(1).section(QLatin1Char(' '), 0, 0).toLower()
                                       : QString();
            const bool l_login_door = f_context.isInLoginPrompt() || l_verb == QStringLiteral("login");
            const bool l_staff_tool = !l_verb.isEmpty() && f_context.isAuthenticated();
            if (!l_login_door && !l_staff_tool) {
                f_context.sendServerMessage("You are OOC muted, and cannot speak.");
                return;
            }
        }

        // The name checks run against the sanitized local; a refused name
        // never commits, so it neither sticks as session state nor sends a
        // player update for a rejected rename.
        static const QRegularExpression l_forbidden("\\[|\\]|\\{|\\}|\\#|\\$|\\%|\\&");
        QString l_name = stripZalgo(l_ooc.name);
        l_name.replace(l_forbidden, "");
        // Impersonation and empty name protection.
        if (l_name.trimmed().replace("​", "").isEmpty() || l_name == f_context.serverNickname()) {
            return;
        }

        if (l_name.size() > 30) {
            f_context.sendServerMessage("Your name is too long! Please limit it to under 30 characters.");
            return;
        }
        f_context.setOocName(l_name);

        // A client in the login prompt is sending its password, not a chat line.
        if (f_context.isInLoginPrompt()) {
            f_context.attemptLogin(l_ooc.message);
            return;
        }

        QString l_message = stripZalgo(l_ooc.message);
        if (l_message.size() == 0 || l_message.size() > f_context.maxMessageLength()) {
            return;
        }

        // The registry chain guards OOC too; only filters registered for
        // the OOC channel run here, which is the word filter by default.
        const auto l_filtered = f_context.applyTextFilters(l_message, TextChannel::Ooc);
        if (!l_filtered) {
            return;
        }
        l_message = *l_filtered;

        if (l_message.at(0) == '/') {
            QStringList l_arguments = l_message.split(" ", Qt::SkipEmptyParts);
            const QString l_command = l_arguments.takeFirst().trimmed().toLower().mid(1);
            f_context.runCommand(l_command, l_arguments);
            return;
        }

        // Commands returned above; only plain chat lines are gated by rules.
        if (const auto l_rule_block = f_context.checkBeforeRule(AreaEvents::OocMessageSent, {{QStringLiteral("message"), l_message}})) {
            f_context.sendServerMessage(*l_rule_block);
            return;
        }

        f_context.broadcastOoc(l_message);
        f_context.runAfterRule(AreaEvents::OocMessageSent, {{QStringLiteral("message"), l_message}});
    }
};

class EvidenceDeleteHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_delete = static_cast<const EvidenceDeleteMessage &>(f_message);

        // Evidence access is enforced by the check_evidence_access floor rule.
        auto l_rule_block = f_context.checkBeforeRule(AreaEvents::EvidenceRemoved, {});
        if (l_rule_block) {
            f_context.sendServerMessage(*l_rule_block);
            return;
        }
        const bool l_deleted = l_delete.index && *l_delete.index >= 0 && *l_delete.index < f_context.evidenceCount();
        if (l_deleted) {
            f_context.deleteEvidence(*l_delete.index);
        }
        f_context.sendEvidenceList();
        if (l_deleted) {
            f_context.runAfterRule(AreaEvents::EvidenceRemoved, {});
        }
    }
};

class EvidenceEditHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_edit = static_cast<const EvidenceEditMessage &>(f_message);

        // Evidence access is enforced by the check_evidence_access floor rule.
        auto l_rule_block = f_context.checkBeforeRule(AreaEvents::EvidenceEdited, {});
        if (l_rule_block) {
            f_context.sendServerMessage(*l_rule_block);
            return;
        }
        if (l_edit.index < 0 || l_edit.index >= f_context.evidenceCount()) {
            return;
        }

        // The hidden-CM owner tagging happens inside the evidence path.
        f_context.replaceEvidence(l_edit.index, l_edit.name, l_edit.description, l_edit.image);
        f_context.sendEvidenceList();
        f_context.runAfterRule(AreaEvents::EvidenceEdited, {});
    }
};

class CasingPreferencesHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_casing = static_cast<const CasingPreferencesMessage &>(f_message);
        if (l_casing.preferences.size() != 5) {
            return;
        }
        f_context.setCasingPreferences(l_casing.preferences);
    }
};

class CaseAnnouncementCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override
    {
        auto l_message = std::make_unique<CaseAnnouncementMessage>();
        l_message->title = f_packet.field(0);
        for (int i = 1; i <= 5; i++) {
            bool l_ok;
            const int l_need = f_packet.field(i).toInt(&l_ok);
            if (!l_ok) {
                l_message->needs.clear();
                l_message->needs_raw.clear();
                return l_message;
            }
            l_message->needs.append(l_need);
            l_message->needs_raw.append(f_packet.field(i));
        }
        return l_message;
    }
};

class CaseAnnouncementHandler : public PacketHandler
{
  public:
    void handle(const Message &f_message, IPacketContext &f_context) const override
    {
        const auto &l_case = static_cast<const CaseAnnouncementMessage &>(f_message);
        if (l_case.needs.size() != 5) {
            return;
        }

        static const QStringList l_roles = {"defense attorney", "prosecutor", "judge", "jurors", "stenographer"};
        QStringList l_needed_roles;
        for (int i = 0; i < 5; i++) {
            if (l_case.needs.at(i)) {
                l_needed_roles.append(l_roles.at(i));
            }
        }
        if (l_needed_roles.isEmpty()) {
            return;
        }

        const QString l_announcer = f_context.oocName().isEmpty() ? f_context.character() : f_context.oocName();
        const QString l_alert = "=== Case Announcement ===\r\n" + l_announcer + " needs " + l_needed_roles.join(", ") +
                                " for " + (l_case.title.isEmpty() ? "a case" : l_case.title) + "!";

        // The trailing 1 is undocumented, but the client rejects a CASEA
        // with fewer than seven fields.
        QStringList l_fields = {l_alert};
        l_fields << l_case.needs_raw << "1";
        f_context.broadcastCaseAlert(l_case.needs, Packet(ao2::HEADER_CASEA, l_fields));
    }
};

} // namespace

void registerChatPackets(PacketRegistry &f_handlers, PacketCodecRegistry &f_codecs)
{
    const QString l_owner = QStringLiteral("core");

    f_handlers.registerHandler({ao2::HEADER_CT, 2, permission::user}, std::make_shared<OocHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_DE, 1, permission::user}, std::make_shared<EvidenceDeleteHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_EE, 4, permission::user}, std::make_shared<EvidenceEditHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_SETCASE, 7, permission::user}, std::make_shared<CasingPreferencesHandler>(), l_owner);
    f_handlers.registerHandler({ao2::HEADER_CASEA, 6, permission::user}, std::make_shared<CaseAnnouncementHandler>(), l_owner);

    f_codecs.registerCodec(ao2::HEADER_CT, always(), 0, std::make_shared<OocCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_DE, always(), 0, std::make_shared<EvidenceDeleteCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_EE, always(), 0, std::make_shared<EvidenceEditCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_SETCASE, always(), 0, std::make_shared<CasingPreferencesCodec>(), l_owner);
    f_codecs.registerCodec(ao2::HEADER_CASEA, always(), 0, std::make_shared<CaseAnnouncementCodec>(), l_owner);
}

} // namespace akashi
