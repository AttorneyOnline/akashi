// AI-generated: written by Claude.
#pragma once

#include "core/text_filter_registry.h"
#include "proto/packet_context.h"

#include <QHash>

namespace akashi {

// Records everything a handler does, so tests can check calls and order.
class FakeContext : public akashi::IPacketContext
{
  public:
    QList<akashi::Packet> sent;
    // Mutable so const context reads (like applyTextFilters) record too.
    mutable QStringList calls;
    bool closed = false;

    int client_id = 5;
    QString stored_hwid;
    akashi::ClientProfile stored_profile;
    bool identified = false;
    bool joined = false;

    int player_count = 3;
    QStringList character_list = {"Phoenix", "Edgeworth", "Maya"};
    QStringList area_name_list = {"Basement", "Courtroom"};
    QStringList music_name_list = {"song1.opus", "song2.opus"};
    akashi::TimerSnapshot global_timer;
    std::optional<akashi::BanRecord> ban = std::nullopt;
    int selected_char_id = -100;

    bool ooc_chat_allowed = true;
    QString ooc_name;
    bool in_login_prompt = false;
    QStringList login_attempts;
    QStringList commands_run;
    QStringList ooc_broadcasts;

    // A false makes selectCharacter refuse like a taken character does.
    bool select_character_result = true;

    int evidence_total = 0;
    QList<int> deleted_evidence;
    QStringList replaced_evidence;
    QList<bool> casing_preferences;

    void sendPacket(const akashi::Packet &f_packet) override
    {
        sent.append(f_packet);
        calls.append("send:" + f_packet.header());
    }

    void sendServerMessage(const QString &f_message) override
    {
        calls.append("message:" + f_message);
    }

    void closeConnection() override
    {
        closed = true;
        calls.append("close");
    }

    int clientId() const override { return client_id; }
    QString hwid() const override { return stored_hwid; }
    const akashi::ClientProfile &profile() const override { return stored_profile; }
    bool isIdentified() const override { return identified; }
    bool isJoined() const override { return joined; }

    void setHwid(const QString &f_hwid) override
    {
        stored_hwid = f_hwid;
        calls.append("setHwid");
    }

    void identify(const akashi::ClientProfile &f_profile) override
    {
        stored_profile = f_profile;
        identified = true;
        calls.append("identify");
    }

    void markJoined() override
    {
        joined = true;
        calls.append("markJoined");
    }

    void finishJoin() override { calls.append("finishJoin"); }
    void logConnectionAttempt() override { calls.append("logConnectionAttempt"); }
    std::optional<akashi::BanRecord> hardwareBan() const override { return ban; }

    int playerCount() const override { return player_count; }
    QStringList characters() const override { return character_list; }
    QStringList musicList() const override { return music_name_list; }
    akashi::TimerSnapshot globalTimer() const override { return global_timer; }

    void announceCharsTaken() override { calls.append("announceCharsTaken"); }
    void sendEvidenceList() override { calls.append("sendEvidenceList"); }
    void broadcastPlayerCount() override { calls.append("broadcastPlayerCount"); }

    bool selectCharacter(int f_char_id) override
    {
        selected_char_id = f_char_id;
        calls.append("selectCharacter");
        return select_character_result;
    }

    bool canUseOocChat() const override { return ooc_chat_allowed; }
    QString oocName() const override { return ooc_name; }

    void setOocName(const QString &f_name) override
    {
        ooc_name = f_name;
        calls.append("setOocName");
    }

    bool isInLoginPrompt() const override { return in_login_prompt; }

    void attemptLogin(const QString &f_message) override
    {
        login_attempts.append(f_message);
        calls.append("attemptLogin");
    }

    // The active-system-id knob: what the AUTH handler validates field 0
    // against and the FL token is built from.
    QString active_auth_system_id = "password";
    QList<QStringList> authenticate_args;

    QString activeAuthSystemId() const override { return active_auth_system_id; }

    void authenticate(const QStringList &f_args) override
    {
        authenticate_args.append(f_args);
        calls.append("authenticate:" + f_args.join(","));
    }

    void runCommand(const QString &f_command, const QStringList &f_arguments) override
    {
        commands_run.append(f_command + "|" + f_arguments.join(","));
        calls.append("runCommand");
    }

    void broadcastOoc(const QString &f_message) override
    {
        ooc_broadcasts.append(f_message);
        calls.append("broadcastOoc");
    }

    int evidenceCount() const override { return evidence_total; }

    void deleteEvidence(int f_index) override
    {
        deleted_evidence.append(f_index);
        calls.append("deleteEvidence");
    }

    void replaceEvidence(int f_index, const QString &f_name, const QString &f_description, const QString &f_image) override
    {
        replaced_evidence = {QString::number(f_index), f_name, f_description, f_image};
        calls.append("replaceEvidence");
    }

    void setCasingPreferences(const QList<bool> &f_preferences) override
    {
        casing_preferences = f_preferences;
        calls.append("setCasingPreferences");
    }

    bool ic_chat_allowed = true;
    bool spectator = false;
    QString current_character = "Phoenix";
    QString current_character_name;
    int current_character_id = 0;
    bool first_person = false;
    QString iniswap;
    QString emote;
    QString offset;
    QString flipping;
    QString last_ic_message;
    QString position;
    QSet<QString> active_filter_ids;
    akashi::TextFilterRegistry *text_filter_registry = nullptr;
    bool ic_message_allowed = true;
    bool area_act_allowed = true;
    bool immediate_forced = false;
    QString area_side;
    QStringList last_area_message;
    akashi::PairInfo pair;
    // Written by the const resolvePair, like the calls list.
    mutable int pair_request = -100;
    int paired_with = -100;
    QStringList broadcast_ic_fields;
    int broadcast_ic_evidence = -100;

    bool canUseIcChat() const override { return ic_chat_allowed; }
    bool isSpectator() const override { return spectator; }
    QString character() const override { return current_character; }
    QString characterName() const override { return current_character_name; }

    void setCharacterName(const QString &f_showname) override
    {
        current_character_name = f_showname;
        calls.append("setCharacterName");
    }

    int characterId() const override { return current_character_id; }
    bool isFirstPerson() const override { return first_person; }
    void setIniswap(const QString &f_character) override { iniswap = f_character; }
    void setEmote(const QString &f_emote) override { emote = f_emote; }
    void setOffset(const QString &f_offset) override { offset = f_offset; }
    void setFlipping(const QString &f_flipping) override { flipping = f_flipping; }
    QString lastIcMessage() const override { return last_ic_message; }
    void setLastIcMessage(const QString &f_message) override { last_ic_message = f_message; }

    void updatePosition(const QString &f_position) override
    {
        position = f_position;
        calls.append("updatePosition");
    }

    std::optional<QString> applyTextFilters(const QString &f_text, akashi::TextChannel f_channel) const override
    {
        calls.append(QString("applyTextFilters:") + (f_channel == akashi::TextChannel::Ooc ? "ooc" : "ic"));
        if (text_filter_registry) {
            return text_filter_registry->apply(f_text, active_filter_ids, f_channel);
        }
        return f_text;
    }
    bool isIcMessageAllowed() const override { return ic_message_allowed; }
    bool canActInArea() const override { return area_act_allowed; }
    bool isImmediateForced() const override { return immediate_forced; }
    QString areaSide() const override { return area_side; }
    QStringList lastAreaMessage() const override { return last_area_message; }

    void setPairingWith(int f_char_id) override
    {
        paired_with = f_char_id;
        calls.append("setPairingWith");
    }

    akashi::PairInfo resolvePair(int f_pair_id) const override
    {
        pair_request = f_pair_id;
        calls.append("resolvePair");
        return pair;
    }

    QStringList applyTestimony(const QStringList &f_fields) override
    {
        calls.append("applyTestimony");
        return f_fields;
    }

    void broadcastIc(const QStringList &f_fields, int f_evidence_index) override
    {
        broadcast_ic_fields = f_fields;
        broadcast_ic_evidence = f_evidence_index;
        calls.append("broadcastIc");
    }

    bool dj_blocked = false;
    bool wtce_blocked = false;
    bool wtce_ready = true;
    // A set refusal makes the matching verb refuse like a rule block.
    QString music_refusal;
    QString wtce_refusal;
    QString penalty_refusal;
    QList<akashi::Packet> area_broadcasts;
    QStringList added_evidence;
    int changed_area = -100;
    int floor_count = 1;
    struct FloorArea
    {
        int floor_id;
        int x;
        int area_id;
    };
    QList<FloorArea> floor_areas = {{0, 0, 0}, {0, 1, 1}};

    bool hasSong(const QString &f_name) const override { return music_name_list.contains(f_name); }
    bool isDjBlocked() const override { return dj_blocked; }

    std::optional<QString> playMusic(const QString &f_song, akashi::MusicSource f_source, const QString &f_char_id, const QString &f_effects) override
    {
        calls.append("playMusic:" + f_song + "|" + akashi::musicSourceName(f_source) + "|" + f_char_id + "|" + f_effects);
        if (!music_refusal.isEmpty())
            return music_refusal;
        return std::nullopt;
    }

    bool isWtceBlocked() const override { return wtce_blocked; }

    bool startWtceCooldown() override
    {
        calls.append("startWtceCooldown");
        return wtce_ready;
    }

    std::optional<QString> useWtce(const QString &f_splash, const std::optional<QString> &f_variant) override
    {
        calls.append("useWtce:" + f_splash + (f_variant ? "|" + *f_variant : QString()));
        if (!wtce_refusal.isEmpty())
            return wtce_refusal;
        return std::nullopt;
    }

    std::optional<QString> changePenalty(int f_bar, int f_value) override
    {
        calls.append("changePenalty:" + QString::number(f_bar) + "|" + QString::number(f_value));
        if (!penalty_refusal.isEmpty())
            return penalty_refusal;
        return std::nullopt;
    }

    void changeArea(int f_area_index) override
    {
        changed_area = f_area_index;
        calls.append("changeArea");
    }

    int floorCount() const override { return floor_count; }
    QStringList floor_area_names;
    QStringList floorAreaNames() const override { return floor_area_names.isEmpty() ? area_name_list : floor_area_names; }
    int floorAreaToGlobal(int f_local_index) const override { return f_local_index; }
    QString before_rule_block;
    // When set, only that event blocks; empty scopes the block to every event.
    QString before_rule_block_event;
    QVariantMap last_before_payload;
    std::optional<QString> checkBeforeRule(const QString &f_event, const QVariantMap &f_payload) override
    {
        calls.append("checkBeforeRule:" + f_event);
        last_before_payload = f_payload;
        if (!before_rule_block.isEmpty() && (before_rule_block_event.isEmpty() || before_rule_block_event == f_event))
            return before_rule_block;
        return std::nullopt;
    }
    // The keys a scripted transform rewrites; they overlay the payload the
    // way runTransforms merges changed keys.
    QVariantMap transform_result;
    QVariantMap last_transform_payload;
    QVariantMap runTransformRules(const QString &f_event, const QVariantMap &f_payload) override
    {
        calls.append("runTransformRules:" + f_event);
        last_transform_payload = f_payload;
        QVariantMap l_result = f_payload;
        for (auto it = transform_result.constBegin(); it != transform_result.constEnd(); ++it)
            l_result.insert(it.key(), it.value());
        return l_result;
    }
    void runAfterRule(const QString &f_event, const QVariantMap &) override { calls.append("runAfterRule:" + f_event); }

    int floorAreaId(int f_floor_id, int f_x) const override
    {
        for (const auto &[fid, x, aid] : floor_areas) {
            if (fid == f_floor_id && x == f_x)
                return aid;
        }
        return -1;
    }

    void addEvidence(const QString &f_name, const QString &f_description, const QString &f_image) override
    {
        added_evidence = {f_name, f_description, f_image};
        calls.append("addEvidence");
    }

    void broadcastArea(const akashi::Packet &f_packet) override
    {
        area_broadcasts.append(f_packet);
        calls.append("broadcastArea:" + f_packet.header());
    }

    QList<bool> case_alert_needs;
    QList<akashi::Packet> case_alerts;
    QString character_password;
    bool authenticated = false;
    QStringList permissions;
    QString current_area_name = "Basement";
    QHash<int, QString> player_names;
    QList<akashi::Packet> moderator_broadcasts;
    QString webhook_reason;
    QStringList kicks;
    QStringList bans;

    void broadcastCaseAlert(const QList<bool> &f_needs, const akashi::Packet &f_packet) override
    {
        case_alert_needs = f_needs;
        case_alerts.append(f_packet);
        calls.append("broadcastCaseAlert");
    }

    void setCharacterPassword(const QString &f_password) override
    {
        character_password = f_password;
        calls.append("setCharacterPassword");
    }

    bool isAuthenticated() const override { return authenticated; }
    bool canPerform(const QString &f_permission) const override { return permissions.contains(f_permission); }
    QString areaName() const override { return current_area_name; }

    std::optional<QString> playerName(int f_client_id) const override
    {
        if (!player_names.contains(f_client_id)) {
            return std::nullopt;
        }
        return player_names.value(f_client_id);
    }

    void broadcastModerators(const akashi::Packet &f_packet) override
    {
        moderator_broadcasts.append(f_packet);
        calls.append("broadcastModerators");
    }

    void recordModcall(const QString &f_reason) override
    {
        webhook_reason = f_reason;
        calls.append("recordModcall");
    }

    void kickPlayer(int f_client_id, const QString &f_reason) override
    {
        kicks.append(QString::number(f_client_id) + "|" + f_reason);
        calls.append("kickPlayer");
    }

    void banPlayer(int f_client_id, int f_duration, const QString &f_reason) override
    {
        bans.append(QString::number(f_client_id) + "|" + QString::number(f_duration) + "|" + f_reason);
        calls.append("banPlayer");
    }

    QString server_nickname = "Server";
    int max_message_length = 256;
    bool webao_enabled = true;
    int max_player_count = 100;
    QString server_description = "A test server.";
    QUrl asset_url = QUrl("http://attorneyoffline.de/base/");
    QString server_motd = "MOTD is not set.";
    QString serverNickname() const override { return server_nickname; }
    int maxMessageLength() const override { return max_message_length; }
    bool webaoEnabled() const override { return webao_enabled; }
    int maxPlayerCount() const override { return max_player_count; }
    QString serverDescription() const override { return server_description; }
    QUrl assetUrl() const override { return asset_url; }
    QString motd() const override { return server_motd; }
};

} // namespace akashi
