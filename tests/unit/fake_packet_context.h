// AI-generated: written by Claude.
#ifndef TESTS_FAKE_PACKET_CONTEXT_H
#define TESTS_FAKE_PACKET_CONTEXT_H

#include "proto/packet_context.h"

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
};

#endif // TESTS_FAKE_PACKET_CONTEXT_H
