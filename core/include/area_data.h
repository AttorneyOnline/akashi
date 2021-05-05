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
#ifndef AREA_DATA_H
#define AREA_DATA_H

#include "logger.h"
#include "aopacket.h"

#include <QMap>
#include <QString>
#include <QSettings>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>

class Logger;

/**
 * @brief Represents an area on the server, a distinct "room" for people to chat in.
 */
class AreaData : public QObject {
  Q_OBJECT
  public:
    /**
     * @brief Constructor for the AreaData class.
     *
     * @param p_name The name of the area. This must be in the format of `"X:YYYYYY"`, where `X` is an integer,
     * and `YYYYYY` is the actual name of the area.
     * @param p_index The index of the area in the area list.
     */
    AreaData(QString p_name, int p_index);

    /**
     * @brief The data for evidence in the area.
     */
    struct Evidence {
        QString name; //!< The name of the evidence, shown when hovered over clientside.
        QString description; //!< The longer description of the evidence, when the user opens the evidence window.
        QString image; //!< A path originating from `base/evidence/` that points to an image file.
    };

    /**
     * @brief The status of an area.
     *
     * @details This is purely aesthetic, and serves no functional purpose from a gameplay perspective.
     * It's only benefit is giving the users a rough idea as to what is going on in an area.
     */
    enum Status {
      IDLE, //!< The area is currently not busy with anything, or the area is empty.
      RP, //!< There is some (non-Ace Attorney-related) roleplay going on in the area.
      CASING, //!< An Ace Attorney or Danganronpa-styled case is currently being held in the area.
      LOOKING_FOR_PLAYERS, //!< Something is being planned in the area, but it needs more players.
      RECESS, //!< The area is currently taking a break from casing, but will continue later.
      GAMING //!< The users inside the area are playing some game outside of AO, and are using the area to communicate.
    };

    /// Exposes the metadata of the Status enum.
    Q_ENUM(Status);

    /**
     * @brief Determines who may traverse and communicate in the area.
     */
    enum LockStatus {
      FREE,
      LOCKED,
      SPECTATABLE
    };

    /**
     * @var LockStatus FREE
     * Anyone may enter the area, and there are no restrictions on communicating in-character.
     */

    /**
     * @var LockStatus LOCKED
     * Only invited clients may enter the area, but those who are invited are free to communicate in-character.
     *
     * When an area transitions from FREE to LOCKED, anyone present in the area
     * at the time of the transition is considered invited.
     */

    /**
     * @var LockStatus SPECTATABLE
     * Anyone may enter the area, but only invited clients may communicate in-character.
     *
     * When an area transitions from FREE to SPECTATABLE, anyone present in the area
     * at the time of the transition is considered invited.
     */

    /// Exposes the metadata of the LockStatus enum.
    Q_ENUM(LockStatus);

    /**
     * @brief The level of "authorisation" needed to be able to modify, add, and remove evidence in the area.
     */
    enum EvidenceMod{
        FFA,
        MOD,
        CM,
        HIDDEN_CM
    };

    /**
     * @var EvidenceMod FFA
     * "Free-for-all" -- anyone can add, remove or modify evidence.
     */

    /**
     * @var EvidenceMod MOD
     * Only mods can add, remove or modify evidence.
     */

    /**
     * @var EvidenceMod CM
     * Only Case Makers and Mods can add, remove or modify evidence.
     */

    /**
     * @var EvidenceMod HIDDEN_CM
     * Only Case Makers and Mods can add, remove or modify evidence.
     *
     * CMs can also hide evidence from various sides by putting `<owner=XXX>` into the evidence's description,
     * where `XXX` is either a position, of a list of positions separated by `,`.
     */

    /**
     * @brief The five "states" the testimony recording system can have in an area.
     */
    enum TestimonyRecording{
        STOPPED,
        RECORDING,
        UPDATE,
        ADD,
        PLAYBACK,
    };

    /**
     * @var TestimonyRecording STOPPED
     * The testimony recorder is inactive and no ic-messages can be played back.
     * If messages are inside the buffer when its stopped, the messages will remain until the recorder is set to RECORDING
     */

    /**
     * @var TestimonyRecording RECORDING
     * The testimony recorder is active and any ic-message send is recorded for playback.
     * It does not differentiate between positions, so any message is recorded. Further improvement?
     * When the recorder is started, it will clear the buffer and will make the first message the title.
     * To prevent accidental recording by not disabling the recorder, a configurable buffer size can be set in the config.
     */

    /**
     * @var TestimonyRecording UPDATE
     * The testimony recorder is active and replaces the current message at the index with the next ic-message
     * Once the IC-Message is send the recorder will default back into playback mode to prevent accidental overwriting of messages.
     */

    /**
     * @var TestimonyRecording ADD
     * The testimony recorder is active and inserts the next message after the currently displayed ic-message
     * This will increase the size by 1.
     */

    /**
     * @var TestimonyRecording PLAYBACK
     * The testimony recorder is inactive and ic-messages in the buffer will be played back.
     */

    /// Exposes the metadata of the TestimonyRecording enum.
    Q_ENUM(TestimonyRecording);

    /**
     * @brief Determines how the testimony progressed after advancement was called in a direction
     * (Either to next or previous statement).
     */
    enum class TestimonyProgress {
        OK, //!< The expected statement was selected.
        LOOPED, //!< The "next" statement would have been beyond the testimony's limits, so the first one was selected.
        STAYED_AT_FIRST, //!< The "previous" statement would have been before the first, so the selection stayed at the first.
    };

    /**
     * @brief Determines a side. Self-explanatory.
     */
    enum class Side {
        DEFENCE, //!< Self-explanatory.
        PROSECUTOR, //!< Self-explanatory.
    };

    /**
     * @brief Contains a list of associations between `/status X` calls and what actual status they set the area to.
     */
    static const QMap<QString, AreaData::Status> map_statuses;

    /**
     * @brief A client in the area has left the area.
     *
     * @details This function counts down the playercount and removes the character from the list of taken characters.
     *
     * @param f_charId The character ID of the client who left. The default value is `-1`. If it is left at that,
     * the area will not try to remove any character from the list of characters taken.
     */
    void clientLeftArea(int f_charId = -1);

    /**
     * @brief A client in the area joined recently.
     *
     * @details This function adds one to the playercount and adds the client's character to the list of taken characters.
     *
     * @param f_charId The character ID of the client who joined. The default value is `-1`. If it is left at that,
     * the area will not add any character to the list of characters taken.
     */
    void clientJoinedArea(int f_charId = -1);

    /**
     * @brief Returns a copy of the list of owners of this area.
     *
     * @return The client IDs of the owners.
     *
     * @see #m_owners
     */
    QList<int> owners() const;

    /**
     * @brief Adds a client to the list of onwers for the area.
     *
     * @details Also automatically adds them to the list of invited people.
     *
     * @param f_clientId The client ID of the client who should be added as an owner.
     *
     * @see #m_owners
     */
    void addOwner(int f_clientId);

    /**
     * @brief Removes the target client from the list of owners.
     *
     * @param f_clientId The ID of the client to remove from the owners.
     *
     * @return True if because of this removal, an ARUP message must be sent out about the locks.
     *
     * @note This function *does not* imply that the client also left the area, only that they are no longer its owner.
     * See clientLeftArea() for that.
     *
     * @see #m_owners
     */
    bool removeOwner(int f_clientId);

    /**
     * @brief Returns true if blankposting is allowed in the area.
     *
     * @return See short description.
     *
     * @see #m_blankpostingAllowed
     */
    bool blankpostingAllowed() const;

    /**
     * @brief Swaps between blankposting being allowed and forbidden in the area.
     *
     * @see #m_blankpostingAllowed
     */
    void toggleBlankposting();

    /**
     * @brief Returns if the area is protected.
     *
     * @return See short description.
     *
     * @see #m_isProtected
     */
    bool isProtected() const;

    /**
     * @brief Returns the lock status of the area.
     *
     * @return See short description.
     *
     * @see #m_locked
     */
    LockStatus lockStatus() const;

    /**
     * @brief Locks the area, setting it to LOCKED.
     */
    void lock();

    /**
     * @brief Unlocks the area, setting it to FREE.
     */
    void unlock();

    /**
     * @brief Sets the area to SPECTATABLE only.
     */
    void spectatable();

    /**
     * @brief Returns the amount of players in the area.
     *
     * @return See short description.
     *
     * @see #m_playerCount
     */
    int playerCount() const;

    /**
     * @brief Returns a copy of the list of timers in the area.
     *
     * @return See short description.
     *
     * @see m_timers
     */
    QList<QTimer *> timers() const;

    /**
     * @brief Returns the name of the area.
     *
     * @return See short description.
     */
    QString name() const;

    /**
     * @brief Returns the index of the area in the server's area list.
     *
     * @return See short description.
     *
     * @todo The area probably shouldn't know its own index.
     */
    int index() const;

    /**
     * @brief Returns a copy of the list of characters taken.
     *
     * @return A list of character IDs.
     *
     * @see #m_charactersTaken
     */
    QList<int> charactersTaken() const;

    /**
     * @brief Adjusts the composition of the list of characters taken, by optionally removing and optionally adding one.
     *
     * @details This function can be used to remove a character, to add one, or to replace one with another (like when a client
     * changes character, hence the name).
     *
     * @param f_from A character ID to remove from the list of characters taken -- a character to switch away "from".
     * Defaults to `-1`. If left at that, no character is removed.
     * @param f_to A character ID to add to the list of characters taken -- a character to switch "to".
     * Defaults to `-1`. If left at that, no character is added.
     *
     * @return True if and only if a character was successfully added to the list of characters taken.
     * False if that character already existed in the list of characters taken, or if `f_to` was left at `-1`.
     * `f_from` does not influence the return value in any way.
     *
     * @todo This is godawful, but I'm at my wits end. Needs a bigger refactor later down the line --
     * the separation should help somewhat already, maybe.
     */
    bool changeCharacter(int f_from = -1, int f_to = -1);

    /**
     * @brief Returns a copy of the list of evidence in the area.
     *
     * @return See short description.
     *
     * @see #m_evidence
     */
    QList<Evidence> evidence() const;

    /**
     * @brief Changes the location of two pieces of evidence in the evidence list to one another's.
     *
     * @param f_eviId1, f_eviId2 The indices of the pieces of evidence to swap.
     */
    void swapEvidence(int f_eviId1, int f_eviId2);

    /**
     * @brief Appends a piece of evidence to the list of evidence.
     *
     * @param f_evi_r The evidence to append.
     */
    void appendEvidence(const Evidence& f_evi_r);

    /**
     * @brief Deletes a piece of evidence from the list of evidence.
     *
     * @param f_eviId The ID of the evidence to delete.
     */
    void deleteEvidence(int f_eviId);

    /**
     * @brief Replaces a piece of evidence at a given position with the one supplied.
     *
     * @param f_eviId The ID of the evidence to replace.
     * @param f_newEvi_r The new piece of evidence that will replace the aforementioned one.
     */
    void replaceEvidence(int f_eviId, const Evidence& f_newEvi_r);

    /**
     * @brief Returns the status of the area.
     *
     * @return See short description.
     */
    Status status() const;

    /**
     * @brief Changes the area of the status to a new one.
     *
     * @param f_newStatus_r A string that a client would enter as an argument for the `/status` command.
     *
     * @return True if the entered status was valid, and the status changed, false otherwise.
     *
     * @see #map_statuses
     */
    bool changeStatus(const QString& f_newStatus_r);

    /**
     * @brief Returns a copy of the list of invited clients.
     *
     * @return A list of client IDs.
     */
    QList<int> invited() const;

    /**
     * @brief Invites a client to the area.
     *
     * @param f_clientId The client ID of the client to invite.
     *
     * @return True if the client was successfully invited. False if they were already in the list of invited people.
     *
     * @see LOCKED and SPECTATABLE for more details about being invited.
     */
    bool invite(int f_clientId);

    /**
     * @brief Removes a client from the list of people invited to the area.
     *
     * @param f_clientId The client ID of the client to uninvite.
     *
     * @return True if the client was successfully uninvited. False if they were never in the list of invited people.
     */
    bool uninvite(int f_clientId);

    /**
     * @brief Returns the name of the background for the area.
     *
     * @return See short description.
     *
     * @see #m_background
     */
    QString background() const;

    /**
     * @brief Returns if custom shownames are allowed in the area.
     *
     * @return See short description.
     *
     * @see #m_shownameAllowed
     */
    bool shownameAllowed() const;

    bool iniswapAllowed() const;

    void toggleIniswap();

    bool bgLocked() const;

    void toggleBgLock();

    QString document() const;

    int defHP() const;

    int proHP() const;

    void changeHP(AreaData::Side f_side, int f_newHP);

    QString currentMusic() const;

    QString musicPlayerBy() const;

    EvidenceMod eviMod() const;

    bool addNotecard(const QString& f_owner_r, const QString& f_notecard_r);

    QStringList getNotecards();

    TestimonyRecording testimonyRecording() const;

    void setTestimonyRecording(const TestimonyRecording &testimonyRecording);

    void restartTestimony();

    void clearTestimony();

    const QVector<QStringList>& testimony() const;

    int statement() const;

    void recordStatement(const QStringList& f_newStatement);

    void addStatement(int f_position, const QStringList& f_newStatement);

    void replaceStatement(int f_position, const QStringList& f_newStatement);

    void removeStatement(int f_statementNumber);

    std::pair<QStringList, TestimonyProgress> advanceTestimony(bool f_forward = true);

    QStringList jumpToStatement(int f_statementNr);

    QStringList judgelog() const;

    void appendJudgelog(const QString& f_newLog_r);

    const QStringList& lastICMessage() const;

    void updateLastICMessage(const QStringList& f_lastMessage);

    bool forceImmediate() const;

    void toggleImmediate();

    bool isMusicAllowed() const;

    void toggleMusic();

    void log(const QString& f_clientName_r, const QString& f_clientIpid_r, const AOPacket& f_packet_r) const;

    void logLogin(const QString &f_clientName_r, const QString &f_clientIpid_r, bool f_success, const QString& f_modname_r) const;

    void flushLogs() const;

    void setEviMod(const EvidenceMod &eviMod);

    QQueue<QString> buffer() const;

private:
    /**
     * @brief The list of timers available in the area.
     */
    QList<QTimer*> m_timers;

    /**
     * @brief The user-facing and internal name of the area.
     */
    QString m_name;

    /**
     * @brief The index of the area in the server's area list.
     */
    int m_index;

    /**
     * @brief A list of the character IDs of all characters taken.
     */
    QList<int> m_charactersTaken;

    /**
     * @brief A list of Evidence currently available in the area's court record.
     *
     * @details This contains *all* evidence, not just the ones a given side can see.
     *
     * @see HIDDEN_CM
     */
    QList<Evidence> m_evidence;

    /**
     * @brief The amount of clients inside the area.
     */
    int m_playerCount;

    /**
     * @brief The status of the area.
     *
     * @see Status
     */
    Status m_status;

    /**
     * @brief The IDs of all the owners (or Case Makers / CMs) of the area.
     */
    QList<int> m_owners;

    /**
     * @brief The list of clients invited to the area.
     *
     * @see LOCKED and SPECTATABLE for the benefits of being invited.
     */
    QList<int> m_invited;

    /**
     * @brief The status of the area's accessibility to clients.
     *
     * @see LockStatus
     */
    LockStatus m_locked;

    /**
     * @brief The background of the area.
     *
     * @details Represents a directory's name in `base/background/` clientside.
     */
    QString m_background;

    /**
     * @brief If true, nobody may become the CM of this area.
     */
    bool m_isProtected;

    /**
     * @brief If true, clients are allowed to put on "shownames", custom names
     * in place of their character's normally displayed name.
     */
    bool m_shownameAllowed;

    /**
     * @brief If true, clients are allowed to use the cursed art of iniswapping in the area.
     */
    bool m_iniswapAllowed;

    /**
     * @brief If true, clients are allowed to send empty IC messages
     */
    bool m_blankpostingAllowed;

    /**
     * @brief If true, the background of the area cannot be changed except by a moderator.
     */
    bool m_bgLocked;

    /**
     * @brief The hyperlink to the document of the area.
     *
     * @details Documents are generally used for cases or roleplays, where they contain the related game's
     * rules. #document can also be something like "None" if there is no case or roleplay being run.
     */
    QString m_document;

    /**
     * @brief The Confidence Gauge's value for the Defence side.
     *
     * @details Unit is 10%, and the values range from 0 (= 0%) to 10 (= 100%).
     */
    int m_defHP;

    /**
     * @brief The Confidence Gauge's value for the Prosecutor side.
     *
     * @copydetails #def_hp
     */
    int m_proHP;

    /**
     * @brief The title of the music currently being played in the area.
     *
     * @details Title is a path to the music file, with the starting point on
     * `base/sounds/music/` clientside, with file extension.
     */
    QString m_currentMusic;

    /**
     * @brief The name of the client (or client's character) that started the currently playing music.
     */
    QString m_musicPlayerBy;

    /**
     * @brief A pointer to a Logger, used to send requests to log data.
     */
    Logger* m_logger;

    /**
     * @brief The evidence mod of the area.
     *
     * @see EvidenceMod
     */
    EvidenceMod m_eviMod;
    QMap<QString, QString> m_notecards;

    TestimonyRecording m_testimonyRecording;


    QVector<QStringList> m_testimony; //!< Vector of all statements saved. Index 0 is always the title of the testimony.
    int m_statement; //!< Keeps track of the currently played statement.

    /**
    * @brief The judgelog of an area.
    *
    * @details This list contains up to 10 recorded packets of the most recent judge actions (WT/CE or penalty updates) in an area.
    */
    QStringList m_judgelog;

    /**
     * @brief The last IC packet sent in an area.
     */
    QStringList m_lastICMessage;


    /**
     * @brief Whether or not to force immediate text processing in this area.
     */
    bool m_forceImmediate;

    /**
     * @brief Whether or not music is allowed in this area. If false, only CMs can change the music.
     */
    bool m_toggleMusic;
};

#endif // AREA_DATA_H