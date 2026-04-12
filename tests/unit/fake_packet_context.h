// AI-generated: written by Claude.
#ifndef TESTS_FAKE_PACKET_CONTEXT_H
#define TESTS_FAKE_PACKET_CONTEXT_H

#include "proto/packet_context.h"

#include <QHash>

// Records everything a handler does, so tests can check calls and order.
class FakeContext : public akashi::IPacketContext
{
  public:
    QList<akashi::Packet> sent;
    QStringList calls;
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
    akashi::AreaSnapshot area;
    akashi::TimerSnapshot global_timer;
    std::optional<akashi::BanRecord> ban;
    int selected_char_id = -100;

    bool ooc_chat_allowed = true;
    QString ooc_name;
    bool in_login_prompt = false;
    QStringList login_attempts;
    QStringList commands_run;
    QStringList ooc_broadcasts;

    bool evidence_access = true;
    bool evidence_hidden_cm = false;
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
    QStringList areaNames() const override { return area_name_list; }
    QStringList musicList() const override { return music_name_list; }
    akashi::AreaSnapshot areaState() const override { return area; }
    akashi::TimerSnapshot globalTimer() const override { return global_timer; }

    void announceCharsTaken() override { calls.append("announceCharsTaken"); }
    void sendEvidenceList() override { calls.append("sendEvidenceList"); }
    void sendFullArup() override { calls.append("sendFullArup"); }
    void broadcastPlayerCount() override { calls.append("broadcastPlayerCount"); }

    bool selectCharacter(int f_char_id) override
    {
        selected_char_id = f_char_id;
        calls.append("selectCharacter");
        return true;
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

    bool canModifyEvidence() override { return evidence_access; }
    bool isEvidenceHiddenCm() const override { return evidence_hidden_cm; }
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
    bool gimped = false;
    bool medieval = false;
    bool shaken = false;
    bool disemvoweled = false;
    bool ic_message_allowed = true;
    bool area_act_allowed = true;
    bool iniswap_allowed = true;
    bool blankposting_allowed = true;
    bool shout_allowed = true;
    bool showname_allowed = true;
    bool immediate_forced = false;
    QString area_side;
    QStringList last_area_message;
    akashi::PairInfo pair;
    int pair_request = -100;
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

    QString gimpText() override { return "I am a heinous criminal."; }
    QString medievalText(const QString &f_text) override { return "Ye olde " + f_text; }
    bool isGimped() const override { return gimped; }
    bool isMedieval() const override { return medieval; }
    bool isShaken() const override { return shaken; }
    bool isDisemvoweled() const override { return disemvoweled; }
    bool isIcMessageAllowed() const override { return ic_message_allowed; }
    bool canActInArea() override { return area_act_allowed; }
    bool isIniswapAllowed() const override { return iniswap_allowed; }
    bool isBlankpostingAllowed() const override { return blankposting_allowed; }
    bool isShoutAllowed() const override { return shout_allowed; }
    bool isShownameAllowed() const override { return showname_allowed; }
    bool isImmediateForced() const override { return immediate_forced; }
    QString areaSide() const override { return area_side; }
    QStringList lastAreaMessage() const override { return last_area_message; }

    akashi::PairInfo resolvePair(int f_pair_id) override
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
    bool music_allowed = true;
    bool jukebox_enabled = false;
    QString jukebox_reply = "Song added to the jukebox.";
    QString queued_jukebox_song;
    QHash<QString, QString> song_aliases;
    QString recorded_music;
    bool wtce_blocked = false;
    bool wtce_allowed = true;
    bool wtce_ready = true;
    QStringList judge_actions;
    QList<akashi::Packet> area_broadcasts;
    QStringList added_evidence;
    int changed_area = -100;

    bool hasSong(const QString &f_name) const override { return music_name_list.contains(f_name); }
    bool isDjBlocked() const override { return dj_blocked; }
    bool isMusicAllowed() const override { return music_allowed; }
    bool isJukeboxEnabled() const override { return jukebox_enabled; }

    QString queueJukeboxSong(const QString &f_song) override
    {
        queued_jukebox_song = f_song;
        calls.append("queueJukeboxSong");
        return jukebox_reply;
    }

    QString resolveSongAlias(const QString &f_song) override
    {
        calls.append("resolveSongAlias");
        return song_aliases.value(f_song, f_song);
    }

    void recordMusicChange(const QString &f_song) override
    {
        recorded_music = f_song;
        calls.append("recordMusicChange");
    }

    bool isWtceBlocked() const override { return wtce_blocked; }
    bool isWtceAllowed() const override { return wtce_allowed; }
    bool startWtceCooldown() override { return wtce_ready; }

    void logJudgeAction(const QString &f_action) override
    {
        judge_actions.append(f_action);
        calls.append("logJudgeAction");
    }

    void changeArea(int f_area_index) override
    {
        changed_area = f_area_index;
        calls.append("changeArea");
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

    QHash<int, int> penalties = {{1, 10}, {2, 10}};
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

    void setPenalty(int f_bar, int f_value) override
    {
        penalties[f_bar] = f_value;
        calls.append("setPenalty");
    }

    int penalty(int f_bar) const override { return penalties.value(f_bar); }

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

    void recordModcall() override { calls.append("recordModcall"); }

    void requestModcallWebhook(const QString &f_reason) override
    {
        webhook_reason = f_reason;
        calls.append("requestModcallWebhook");
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
};

#endif // TESTS_FAKE_PACKET_CONTEXT_H
