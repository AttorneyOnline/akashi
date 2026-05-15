//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#ifndef AOCLIENT_H
#define AOCLIENT_H

#include "acl_roles_handler.h"
#include "akashi_core_export.h"
#include "proto/packet_context.h"

#include <QDateTime>
#include <QHostAddress>
#include <QRegularExpression>
#include <QTimer>
#include <QtGlobal>

#include <algorithm>
#include <memory>
#include <optional>

class AreaData;
class DBManager;
class Server;

namespace akashi {
enum class DisconnectKind;
class ClientSession;
class ITransport;
class PlayerState;
struct PacketSpec;
}

/**
 * @brief Represents a client connected to the server running Attorney Online 2 or one of its derivatives.
 */
class AKASHI_CORE_EXPORT AOClient : public QObject, public akashi::IPacketContext
{
    Q_OBJECT

  public:
    /**
     * @brief Creates an instance of the AOClient class.
     *
     * @param p_server A pointer to the Server instance where the client is joining to.
     * @param p_socket The socket associated with the AOClient.
     * @param user_id The user ID of the client.
     * @param parent Qt-based parent, passed along to inherited constructor from QObject.
     */
    AOClient(Server *p_server, akashi::ITransport *socket, QObject *parent = nullptr, int user_id = 0);

    /**
     * @brief Destructor for the AOClient instance.
     *
     * @details Runs leave() as a safety net; the server normally calls it
     * explicitly before deletion.
     */
    ~AOClient();

    /**
     * @brief Getter for the client's IPID.
     *
     * @return The IPID.
     *
     * @see #ipid
     */
    QString ipid() const;

    /**
     * @brief Returns true if the client has completed the participation handshake. False otherwise.
     *
     * @return True if the client has completed the participation handshake. False otherwise.
     */
    bool isJoined() const;

    /**
     * @brief Returns true if the client has logged-in as a role.
     *
     * @return True if loggged-in, false otherwise.
     */
    bool isAuthenticated() const override;

    /**
     * @brief Calculates the client's IPID based on a hashed version of its IP.
     */
    void calculateIpid();

    /**
     * @brief Getter for the pointer to the server.
     *
     * @return See brief description.
     *
     * @note Unused. There isn't really a point to this existing, either.
     *
     * @see #server
     */
    Server *server();

    int clientId() const;

    /**
     * @brief The characters this connection plays; legacy clients have
     * exactly one. Each is its own entry in the player list.
     */
    QList<akashi::PlayerState *> players() const;

    /**
     * @brief Keeps the client in the server for f_seconds after a lost
     * connection, in case the person comes right back. reconnectTimedOut()
     * fires if they do not.
     */
    void waitForReconnect(int f_seconds);

    bool isWaitingForReconnect() const;

    QString name() const;
    void setName(const QString &f_name);

    QString character() const;
    void setCharacter(const QString &f_character);

    QString characterName() const;
    void setCharacterName(const QString &f_showname);

    int areaId() const;
    void setAreaId(const int f_area_id);

    // Timed moderation sanctions, toggled by the moderation commands and
    // read by the chat/music/judge packet handlers. Per-session today; a
    // persistent sanction store backs these in M6. The isDjBlocked() and
    // isWtceBlocked() getters live with the packet-context overrides below.
    bool isMuted() const;
    void setMuted(bool f_muted);

    bool isOocMuted() const;
    void setOocMuted(bool f_ooc_muted);

    void setDjBlocked(bool f_dj_blocked);

    void setWtceBlocked(bool f_wtce_blocked);

    /**
     * @brief Checks if the client's ACL role has permission for the given permission.
     *
     * @param f_permission The permission flags.
     *
     * @return True if the client has permission, false otherwise.
     */
    bool canPerform(ACLRole::Permission f_permission) const;

    /**
     * @brief Returns if the client is a spectator.
     *
     * @return True if the client is a spectator, false otherwise.
     */
    bool isSpectator() const;

    /**
     * @brief Sets the spectator state for the client.
     *
     * @param f_spectator
     */
    void setSpectator(bool f_spectator);

    /**
     * @brief Sends an out-of-character message originating from the server to the client.
     *
     * @param message The text of the message to send.
     */
    void sendServerMessage(const QString &message) override;

    /**
     * @brief Like with AOClient::sendServerMessage(), but to every client in the client's area.
     *
     * @param message The text of the message to send.
     */
    void sendServerMessageArea(QString message);

    /**
     * @brief Like with AOClient::sendServerMessage(), but to every client in the server.
     *
     * @param message The text of the message to send.
     */
    void sendServerBroadcast(QString message);

    /**
     * @brief Calls AOClient::updateEvidenceList() for every client in the current client's area.
     *
     * @param area The current client's area.
     */
    void sendEvidenceList(AreaData *area) const;

    /**
     * @brief Updates the evidence list in the area for the client.
     *
     * @param area The client's area.
     */
    void updateEvidenceList(AreaData *area);

    /**
     * @brief Removes excessive combining characters from a text.
     *
     * @param p_text The text to clear of its excessive combining characters.
     *
     * @return See brief description.
     *
     * @see https://en.wikipedia.org/wiki/Zalgo_text
     */
    QString dezalgo(QString p_text);

    /**
     * @brief Checks if the client can modify the evidence in the area.
     *
     * @param area The client's area.
     *
     * @return True if the client can modify the evidence, false if not.
     */
    bool canModifyEvidence(AreaData *area);

    /**
     * @brief Changes the client's character.
     *
     * @param char_id The character ID of the client's new character.
     */
    bool changeCharacter(int char_id);

    /**
     * @brief A helper function for logging in a client as moderator.
     *
     * @param message The OOC message the client has sent.
     */
    void loginAttempt(QString message);

    /**
     * @brief Changes the area the client is in.
     *
     * @param new_area The ID of the new area.
     */
    void changeArea(int new_area) override;

    /**
     * @brief Handles an incoming command, checking for authorisation and minimum argument count.
     *
     * @param command The incoming command.
     * @param argc The amount of arguments the command was called with. Equivalent to `argv.size()`.
     * @param argv The arguments the command was called with.
     */
    void handleCommand(QString command, int argc, QStringList argv);

    /**
     * @brief A helper function for decoding AO encoding from a QString.
     *
     * @param incoming_message QString to be decoded.
     */
    QString decodeMessage(QString incoming_message);

    /**
     * @brief Adds the last send IC-Message to QVector of the respective area.
     *
     * @details This one pulls double duty to both append IC-Messages to the QVector or insert them, depending on the current recorder enum.
     *
     * @param packet The MS-Packet being recorded with their color changed to green.
     */
    void addStatement(QStringList packet);

    /**
     * @brief Updates the currently displayed IC-Message with the next one send
     * @param packet The IC-Message that will overwrite the currently stored one.
     * @return Returns the updated IC-Message to be send to the other users. It also changes the color to green.
     */
    QStringList updateStatement(QStringList packet);

    /**
     * @brief Convenience function to generate a random integer number between the given minimum and maximum values.
     *
     * @param min The minimum possible value for the random integer, inclusive.
     * @param max The maximum possible value for the random integer, exclusive.
     *
     * @warning `max` must be greater than `min`.
     *
     * @return A randomly generated integer within the bounds given.
     */
    int genRand(int min, int max);

    /**
     * @brief A helper function to add recorded packets to an area's judgelog.
     *
     * @param area Pointer to the area where the packet was sent.
     *
     * @param client Pointer to the client that sent the packet.
     *
     * @param action String containing the info that is being recorded.
     */
    void updateJudgeLog(AreaData *area, AOClient *client, QString action);

    /**
     * @brief The spectator character ID
     *
     * @details You may assume that AO has a sane way to determine if a user is a spectator
     * or an actual player. Well, to nobodys surprise, this is not the case, so the character id -1 is used
     * to determine if a client has entered spectator or user mode. I am making this a const mostly
     * for the case this could change at some point in the future, but don't count on it.
     */
    const int SPECTATOR_ID = -1;

    // The akashi::IPacketContext view of this client, used by packet handlers.
    // This is the seam the session classes grow out of later.
    void closeConnection() override;
    QString hwid() const override;
    const akashi::ClientProfile &profile() const override;
    bool isIdentified() const override;
    void setHwid(const QString &f_hwid) override;
    void identify(const akashi::ClientProfile &f_profile) override;
    void markJoined() override;
    void finishJoin() override;
    void logConnectionAttempt() override;
    std::optional<akashi::BanRecord> hardwareBan() const override;
    QString serverNickname() const override;
    int maxMessageLength() const override;
    QStringList wordFilters() const override;
    bool webaoEnabled() const override;
    int maxPlayerCount() const override;
    QString serverDescription() const override;
    QUrl assetUrl() const override;
    QString motd() const override;
    DataTypes::AuthType packetAuthType() const override;
    int messageFloodguardMs() const override;
    int globalMessageFloodguardMs() const override;
    bool isDiscordBanEnabled() const override;
    bool isDiscordModcallEnabled() const override;
    int playerCount() const override;
    QStringList characters() const override;
    QStringList areaNames() const override;
    QStringList musicList() const override;
    akashi::AreaSnapshot areaState() const override;
    akashi::TimerSnapshot globalTimer() const override;
    void announceCharsTaken() override;
    void sendEvidenceList() override;
    void sendFullArup() override;
    void broadcastPlayerCount() override;
    bool selectCharacter(int f_char_id) override;
    bool canUseOocChat() const override;
    QString oocName() const override;
    void setOocName(const QString &f_name) override;
    bool isInLoginPrompt() const override;
    void attemptLogin(const QString &f_message) override;
    void runCommand(const QString &f_command, const QStringList &f_arguments) override;
    void broadcastOoc(const QString &f_message) override;
    bool canModifyEvidence() override;
    bool isEvidenceHiddenCm() const override;
    int evidenceCount() const override;
    void deleteEvidence(int f_index) override;
    void replaceEvidence(int f_index, const QString &f_name, const QString &f_description, const QString &f_image) override;
    void setCasingPreferences(const QList<bool> &f_preferences) override;
    bool canUseIcChat() const override;
    int characterId() const override;
    bool isFirstPerson() const override;
    void setIniswap(const QString &f_character) override;
    void setEmote(const QString &f_emote) override;
    void setOffset(const QString &f_offset) override;
    void setFlipping(const QString &f_flipping) override;
    QString iniswap() const;
    QString emote() const;
    QString offset() const;
    QString flipping() const;
    QString pos() const;
    int pairingWith() const;
    void setPairingWith(int f_char_id);
    QString lastIcMessage() const override;
    void setLastIcMessage(const QString &f_message) override;
    void updatePosition(const QString &f_position) override;
    QString gimpText() override;
    QString medievalText(const QString &f_text) override;
    bool isGimped() const override;
    bool isMedieval() const override;
    bool isMedievalArea() const override;
    bool isShaken() const override;
    bool isDisemvoweled() const override;
    void setGimped(bool f_gimped);
    void setMedieval(bool f_medieval);
    void setShaken(bool f_shaken);
    void setDisemvoweled(bool f_disemvoweled);

    bool isAfk() const;
    void setAfk(bool f_afk);

    bool isPmMuted() const;
    void setPmMuted(bool f_pm_muted);

    bool isAdvertEnabled() const;
    void setAdvertEnabled(bool f_advert_enabled);

    bool isCharCursed() const;
    void setCharCursed(bool f_char_cursed);

    bool isTestimonySaving() const;
    void setTestimonySaving(bool f_testimony_saving);

    QHostAddress remoteIp() const;
    QString moderatorName() const;
    void setModeratorName(const QString &f_name);
    QString aclRoleId() const;
    void setAclRoleId(const QString &f_role_id);
    void setAuthenticated(bool f_state);

    void setInLoginPrompt(bool f_in_login_prompt);
    void setCharacterId(int f_char_id);
    void setFirstPerson(bool f_first_person);
    bool isGlobalEnabled() const;
    void setGlobalEnabled(bool f_global_enabled);
    QList<bool> casingPreferences() const;
    QList<int> charCurseList() const;
    void addCharCurse(int f_char_id);
    void clearCharCurse();
    void changePosition(QString new_pos);
    void closeSocket();
    bool isIcMessageAllowed() const override;
    bool canActInArea() override;
    bool isIniswapAllowed() const override;
    bool isBlankpostingAllowed() const override;
    bool isShoutAllowed() const override;
    bool isShownameAllowed() const override;
    bool isImmediateForced() const override;
    QString areaSide() const override;
    QStringList lastAreaMessage() const override;
    akashi::PairInfo resolvePair(int f_pair_id) override;
    QStringList applyTestimony(const QStringList &f_fields) override;
    void broadcastIc(const QStringList &f_fields, int f_evidence_index) override;
    bool hasSong(const QString &f_name) const override;
    bool isDjBlocked() const override;
    bool isMusicAllowed() const override;
    bool isJukeboxEnabled() const override;
    QString queueJukeboxSong(const QString &f_song) override;
    QString resolveSongAlias(const QString &f_song) override;
    void recordMusicChange(const QString &f_song) override;
    bool isWtceBlocked() const override;
    bool isWtceAllowed() const override;
    bool startWtceCooldown() override;
    void logJudgeAction(const QString &f_action) override;
    void addEvidence(const QString &f_name, const QString &f_description, const QString &f_image) override;
    void broadcastArea(const akashi::Packet &f_packet) override;
    void setPenalty(int f_bar, int f_value) override;
    int penalty(int f_bar) const override;
    void broadcastCaseAlert(const QList<bool> &f_needs, const akashi::Packet &f_packet) override;
    void setCharacterPassword(const QString &f_password) override;
    bool canPerform(const QString &f_permission) const override;
    QString areaName() const override;
    std::optional<QString> playerName(int f_client_id) const override;
    void broadcastModerators(const akashi::Packet &f_packet) override;
    void recordModcall() override;
    void requestModcallWebhook(const QString &f_reason) override;
    void kickPlayer(int f_client_id, const QString &f_reason) override;
    void banPlayer(int f_client_id, int f_duration, const QString &f_reason) override;

  public Q_SLOTS:
    /**
     * @brief Handles an incoming packet, checking for authorisation and minimum argument count.
     *
     * @param packet The incoming packet.
     */
    void handlePacket(const akashi::Packet &packet);

    /**
     * @brief Withdraws the client's presence from the server: area roster,
     * ARUP counts, invites and CM spots. Idempotent, and must run while the
     * client is alive - never from the destructor doing real work.
     */
    void leave();

    /**
     * @brief A slot for sending a packet to the client.
     *
     * @param packet The packet to send.
     */
    void sendPacket(const akashi::Packet &packet) override;

    /**
     * @overload
     */
    void sendPacket(QString header, QStringList contents);

    /**
     * @overload
     */
    void sendPacket(QString header);

    /**
     * @brief A slot for when the client's AFK timer runs out.
     */
    void onAfkTimeout();

  Q_SIGNALS:
    /**
     * @brief This signal is emitted when the client has completed the participation handshake.
     */
    void joined();

    /**
     * @brief The client's connection has closed; the server decides on this
     * whether to tear the client down or wait for a reconnect.
     */
    void disconnected(akashi::DisconnectKind f_kind);

    /**
     * @brief The reconnect wait ran out; the server tears the client down on this.
     */
    void reconnectTimedOut();

  private:

    /**
     * @brief The connection and the person behind it - owns the transport,
     * the packet pipeline, the PlayerStates, identity, auth, sanctions and
     * preferences. An owned child, so it lives exactly as long as the client.
     */
    akashi::ClientSession *m_session;

    /**
     * @brief Set once leave() has run, making it idempotent.
     */
    bool m_left = false;

    /**
     * @brief The session's active character - the one the classic
     * protocol addresses.
     */
    akashi::PlayerState *player() const;

    /**
     * @brief A pointer to the Server, used for updating server variables that depend on the client (e.g. amount of players in an area).
     */
    Server *m_server;

    /**
     * @brief Runs a packet through its registered handler after the usual checks.
     */
    void handleRegisteredPacket(const akashi::Packet &f_packet, const akashi::PacketSpec &f_spec);

    /**
     * @brief Marks the client active again; any packet except the keepalive counts.
     */
    void resetAfk(const QString &f_header);

    bool checkTestimonySymbols(const QString &message);

  Q_SIGNALS:

    /**
     * @brief Signal connected to universal logger. Sends IC chat usage to the logger.
     */
    void logIC(const QString &f_areaName, const QString &f_ipid, const QString &f_oocName, const QString &f_id, const QString &f_charName, const QString &f_message);

    /**
     * @brief Signal connected to universal logger. Sends music usage to the logger.
     */
    void logMusic(const QString &f_charName, const QString &f_oocName, const QString &f_ipid,
                  const QString &f_areaName, const QString &f_track);

    /**
     * @brief Signal connected to universal logger. Sends OOC chat usage to the logger.
     */
    void logOOC(const QString &f_areaName, const QString &f_ipid, const QString &f_oocName, const QString &f_id, const QString &f_charName, const QString &f_message);

    /**
     * @brief Signal connected to universal logger. Sends login attempt to the logger.
     */
    void logLogin(const QString &f_charName, const QString &f_oocName, const QString &f_moderatorName,
                  const QString &f_ipid, const QString &f_areaName, const bool &f_success);

    /**
     * @brief Signal connected to universal logger. Sends command usage to the logger.
     */
    void logCMD(const QString &f_charName, const QString &f_ipid, const QString &f_oocName, const QString f_command,
                const QStringList f_args, const QString f_areaName);

    /**
     * @brief Signal connected to universal logger. Sends player kick information to the logger.
     */
    void logKick(const QString &f_moderator, const QString &f_targetIPID, const QString &f_reason);

    /**
     * @brief Signal connected to universal logger. Sends ban information to the logger.
     */
    void logBan(const QString &f_moderator, const QString &f_targetIPID, const QString &f_duration, const QString &f_reason);

    /**
     * @brief Signal connected to universal logger. Sends modcall information to the logger, triggering a write of the buffer
     *        when modcall logging is used.
     */
    void logModcall(const QString &f_area_name, const QString &f_ipid, const QString &f_ooc_name, const QString &f_id, const QString &f_char_name);
};

#endif // AOCLIENT_H
