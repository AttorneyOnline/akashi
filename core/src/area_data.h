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

#include <QDebug>
#include <QElapsedTimer>
#include <QMap>
#include <QRandomGenerator>
#include <QSettings>
#include <QString>
#include <QTimer>

#include "include/network/aopacket.h"

class ConfigManager;
class Logger;
class MusicManager;
class AOPacket;

/**
 * @brief Represents an area on the server, a distinct "room" for people to chat in.
 */
class AreaData : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor for the AreaData class.
     *
     * @param p_name The name of the area. This must be in the format of `"X:YYYYYY"`, where `X` is an integer,
     * and `YYYYYY` is the actual name of the area.
     * @param p_index The index of the area in the area list.
     */
    AreaData(QString p_name, int p_index, MusicManager *p_music_manager);

    /**
     * @brief The data for evidence in the area.
     */
    struct Evidence
    {
        QString name;        //!< The name of the evidence, shown when hovered over clientside.
        QString description; //!< The longer description of the evidence, when the user opens the evidence window.
        QString image;       //!< A path originating from `base/evidence/` that points to an image file.
    };

    /**
     * @brief The status of an area.
     *
     * @details This is purely aesthetic, and serves no functional purpose from a gameplay perspective.
     * It's only benefit is giving the users a rough idea as to what is going on in an area.
     */
    enum Status
    {
        IDLE,                //!< The area is currently not busy with anything, or the area is empty.
        RP,                  //!< There is some (non-Ace Attorney-related) roleplay going on in the area.
        CASING,              //!< An Ace Attorney or Danganronpa-styled case is currently being held in the area.
        LOOKING_FOR_PLAYERS, //!< Something is being planned in the area, but it needs more players.
        RECESS,              //!< The area is currently taking a break from casing, but will continue later.
        GAMING               //!< The users inside the area are playing some game outside of AO, and are using the area to communicate.
    };

    /// Exposes the metadata of the Status enum.
    Q_ENUM(Status);

    /**
     * @brief Determines who may traverse and communicate in the area.
     */
    enum LockStatus
    {
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
    enum class EvidenceMod
    {
        FFA,
        MOD,
        CM,
        HIDDEN_CM
    };
    Q_ENUM(EvidenceMod)

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
    enum TestimonyRecording
    {
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
    enum class TestimonyProgress
    {
        OK,              //!< The expected statement was selected.
        LOOPED,          //!< The "next" statement would have been beyond the testimony's limits, so the first one was selected.
        STAYED_AT_FIRST, //!< The "previous" statement would have been before the first, so the selection stayed at the first.
    };

    /**
     * @brief Determines a side. Self-explanatory.
     */
    enum class Side
    {
        DEFENCE,    //!< Self-explanatory.
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
     *
     * @param f_userId The user ID of the client who left. The default value is '-1', This ID is technically
     * impossible.
     */
    void clientLeftArea(int f_charId = -1, int f_userId = -1);

    /**
     * @brief A client in the area joined recently.
     *
     * @details This function adds one to the playercount and adds the client's character to the list of taken characters.
     *
     * @param f_charId The character ID of the client who joined. The default value is `-1`. If it is left at that,
     * the area will not add any character to the list of characters taken.
     *
     * @param f_userId The user ID of the client who left. The default value is '-1', This ID is technically
     * impossible.
     */
    void clientJoinedArea(int f_charId = -1, int f_userId = -1);

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
     * @brief Returns the jukebox status of the area.
     *
     * @return See short description.
     *
     * @see #m_jukebox
     */
    bool isjukeboxEnabled() const;

    /**
     * @brief Returns the amount of songs pending in the Jukebox queue.
     *
     * @return Remaining entries of the queue as int.
     */
    int getJukeboxQueueSize() const;

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
    void appendEvidence(const Evidence &f_evi_r);

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
    void replaceEvidence(int f_eviId, const Evidence &f_newEvi_r);

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
    bool changeStatus(const QString &f_newStatus_r);

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
     * @brief Sets the background of the area.
     *
     * @see #AOClient::cmdSetBackground and #m_background
     */
    void setBackground(const QString f_background);

    /**
     * @brief Returns if custom shownames are allowed in the area.
     *
     * @return See short description.
     *
     * @see #m_shownameAllowed
     */
    bool shownameAllowed() const;

    /**
     * @brief Returns if iniswapping is allowed in the area.
     *
     * @return See short description.
     *
     * @see #m_iniswapAllowed
     */
    bool iniswapAllowed() const;

    /**
     * @brief Toggles whether iniswap is allowed in the area.
     *
     * @see #m_iniswapAllowed
     */
    void toggleIniswap();

    /**
     * @brief Returns if backgrounds changing is locked in the area.
     *
     * @return See short description.
     *
     * @see #m_bgLocked
     */
    bool bgLocked() const;

    /**
     * @brief Toggles whether backgrounds changing is allowed in the area.
     *
     * @see #m_bgLocked
     */
    void toggleBgLock();

    /**
     * @brief Returns the document of the area.
     *
     * @return See short description.
     *
     * @see #m_document
     */
    QString document() const;

    /**
     * @brief Changes the document in the area.
     *
     * @param f_newDoc_r The new document.
     *
     * @see #m_document
     */
    void changeDoc(const QString &f_newDoc_r);

    /**
     * @brief Returns the message of the area.
     *
     * @return See short description.
     *
     * @see #m_area_message
     */
    QString areaMessage() const;

    /**
     * @brief Returns if the area's message should be sent when a user joins the area.
     *
     * @return See short description.
     */
    bool sendAreaMessageOnJoin() const;

    /**
     * @brief Changes the area message in the area.
     *
     * @param f_newMessage_r The new message.
     */
    void changeAreaMessage(const QString &f_newMessage_r);

    /**
     * @brief Clear the area message in the area.
     */
    void clearAreaMessage();

    /**
     * @brief Returns the value of the Confidence bar for the defence's side.
     *
     * @return The value of the Confidence bar in units of 10%.
     *
     * @see #m_defHP
     */
    int defHP() const;

    /**
     * @brief Returns the value of the Confidence bar for the prosecution's side.
     *
     * @return The value of the Confidence bar in units of 10%.
     *
     * @see #m_proHP
     */
    int proHP() const;

    /**
     * @brief Changes the value of the Confidence bar for the given side.
     *
     * @param f_side The side whose Confidence bar to change.
     * @param f_newHP The absolute new value for the Confidence bar.
     * Will be clamped between 0 and 10, inclusive on both sides.
     */
    void changeHP(AreaData::Side f_side, int f_newHP);

    /**
     * @brief Returns the music currently being played in the area.
     *
     * @return See short description.
     *
     * @see #m_currentMusic
     */
    QString currentMusic() const;

    /**
     * @brief Sets the music currently being played in the area.
     *
     * @param Name of the song being played.
     *
     * @see #m_currentMusic
     */
    void setCurrentMusic(QString f_current_song);

    /**
     * @brief Returns the showname of the client who played the music in the area.
     *
     * @return See short description.
     *
     * @see #m_musicPlayedBy
     */
    QString musicPlayerBy() const;

    /**
     * @brief Sets the showname of the client who played the music in the area.
     *
     * @param Showname of the client.
     *
     * @see #m_musicPlayedBy
     */
    void setMusicPlayedBy(const QString &f_music_player);

    /**
     * @brief Changes the music being played in the area.
     *
     * @param f_source_r The showname of the client who initiated the music change.
     * @param f_newSong_r The name of the new song that is going to be played in the area.
     */
    void changeMusic(const QString &f_source_r, const QString &f_newSong_r);

    /**
     * @brief Returns the evidence mod in the area.
     *
     * @return See short description.
     *
     * @see #m_eviMod
     */
    EvidenceMod eviMod() const;

    /**
     * @brief Sets the evidence mod in the area.
     *
     * @param f_eviMod_r The new evidence mod.
     */
    void setEviMod(const EvidenceMod &f_eviMod_r);

    /**
     * @brief Adds a notecard to the area.
     *
     * @param f_owner_r The showname of the character to whom the notecard should be associated to.
     * @param f_notecard_r The contents of the notecard.
     *
     * @return True if the notecard didn't replace a previous one, false if it did.
     */
    bool addNotecard(const QString &f_owner_r, const QString &f_notecard_r);

    /**
     * @brief Returns the list of notecards recorded in the area.
     *
     * @return Returns a QStringList with the format of `name: message`, with newlines at the end
     * of each message.
     */
    QStringList getNotecards();

    /**
     * @brief Returns the state of the testimony recording process in the area.
     *
     * @return See short description.
     */
    TestimonyRecording testimonyRecording() const;

    /**
     * @brief Sets the state of the testimony recording process in the area.
     *
     * @param f_testimonyRecording_r The new state for testimony recording.
     */
    void setTestimonyRecording(const TestimonyRecording &f_testimonyRecording_r);

    /**
     * @brief Sets the testimony to the first moment, and the state to TestimonyRecording::PLAYBACK.
     */
    void restartTestimony();

    /**
     * @brief Clears the testimony, sets the state to TestimonyRecording::STOPPED, and the statement
     * index to -1.
     */
    void clearTestimony();

    /**
     * @brief Returns the contents of the testimony.
     *
     * @return A const reference to the testimony.
     *
     * @note Unlike most other getters, this one returns a reference, as it is expected to be used frequently.
     */
    const QVector<QStringList> &testimony() const;

    /**
     * @brief Returns the index of the currently examined statement in the testimony.
     *
     * @return See short description.
     */
    int statement() const;

    /**
     * @brief Adds a new statement to the end of the testimony, and increases the statement index by one.
     *
     * @param f_newStatement_r The IC message packet to append to the testimony vector.
     */
    void recordStatement(const QStringList &f_newStatement_r);

    /**
     * @brief Adds a statement into the testimony to a given position.
     *
     * @param f_position The index to insert the statement to.
     * @param f_newStatement_r The IC message packet to insert.
     */
    void addStatement(int f_position, const QStringList &f_newStatement_r);

    /**
     * @brief Replaces an already existing statement in the testimony in a given position with a new one.
     *
     * @param f_position The index of the statement to replace.
     * @param f_newStatement_r The IC message packet to insert in the old one's stead.
     */
    void replaceStatement(int f_position, const QStringList &f_newStatement_r);

    /**
     * @brief Removes a statement from the testimony at a given position, and moves the statement index one backward.
     *
     * @param f_position The index to remove the statement from.
     */
    void removeStatement(int f_position);

    /**
     * @brief Jumps the testimony playback to the given index.
     *
     * @details When advancing forward, if the playback would go past the last statement,
     * it instead returns the first statement.
     * When advancing backward, if the playback would go before the first statement, it
     * instead returns the first statement.
     *
     * @param f_position The index to jump to.
     *
     * @return A pair of values:
     * * First, a `QStringList` that is the packet of the statement that was advanced to.
     * * Then, a `TestimonyProgress` value that describes how the advancement happened.
     */
    QPair<QStringList, AreaData::TestimonyProgress> jumpToStatement(int f_position);

    /**
     * @brief Returns a copy of the judgelog in the area.
     *
     * @return See short description.
     *
     * @see #m_judgelog
     */
    QStringList judgelog() const;

    /**
     * @brief Appends a new line to the judgelog.
     *
     * @details There is a hard limit of 10 lines in the judgelog -- if a new one is inserted
     * beyond that, the oldest one is cleared.
     *
     * @param f_newLog_r The new line to append to the judgelog.
     */
    void appendJudgelog(const QString &f_newLog_r);

    /**
     * @brief Returns the last IC message sent in the area.
     *
     * @return See short description.
     */
    const QStringList &lastICMessage() const;

    /**
     * @brief Updates the last IC message sent in the area.
     *
     * @param f_lastMessage_r The new last IC message.
     */
    void updateLastICMessage(const QStringList &f_lastMessage_r);

    /**
     * @brief Returns whether ~~non-interrupting~~ immediate messages are forced in the area.
     *
     * @return See short description.
     *
     * @see #m_forceImmediate
     */
    bool forceImmediate() const;

    /**
     * @brief Toggles whether immediate messages are forced in the area.
     */
    void toggleImmediate();

    /**
     * @brief Returns whether changing music is allowed in the area.
     *
     * @return See short description.
     *
     * @see #m_toggleMusic
     */
    bool isMusicAllowed() const;

    /**
     * @brief Toggles whether changing music is allowed in the area.
     */
    void toggleMusic();

    /**
     * @brief Returns whether the BG list is ignored in this araa.
     *
     * @return See short description.
     */
    bool ignoreBgList();

    /**
     * @brief Toggles whether the BG list is ignored in this area.
     */
    void toggleIgnoreBgList();

    /**
     * @brief Toggles whether the area message is sent upon joining the area.
     */
    void toggleAreaMessageJoin();

    /**
     * @brief Toggles wether the jukebox is enabled or not.
     */
    void toggleJukebox();

    /**
     * @brief Toggles wether testimony animations can be used in the area.
     */
    void toggleWtceAllowed();

    /**
     * @brief Toggles wether shouts can be used in the area.
     */
    void toggleShoutAllowed();

    /**
     * @brief Adds a song to the Jukeboxs queue.
     */
    QString addJukeboxSong(QString f_song);

    /**
     * @brief Returns a constant that includes all currently joined userids.
     */
    QVector<int> joinedIDs() const;

    /**
     * @brief Returns whether a game message may be broadcasted or not.
     *
     * @return True if expired; false otherwise.
     */
    bool isMessageAllowed() const;

    /**
     * @brief Returns whether testimony animation packets may be broadcasted or not.
     *
     * @return True if permitted, false otherwise.
     */
    bool isWtceAllowed() const;

    /**
     * @brief Returns whether a shout can be used in the area.
     *
     * @return True if permitted, false otherwise.
     */
    bool isShoutAllowed() const;

    /**
     * @brief Starts a timer that determines whether a game message may be broadcasted or not.
     *
     * @param f_duration The duration of the message floodguard timer.
     */
    void startMessageFloodguard(int f_duration);

  public slots:

    /**
     * @brief Plays a random song from the jukebox. Plays the same if only one is left.
     */
    void switchJukeboxSong();

  signals:

    /**
     * @brief Sends a packet to every client inside the area.
     */
    void sendAreaPacket(AOPacket *f_packet, int f_area_index);

    /**
     * @brief sendAreaPacketClient Sends a packet to the specified client.
     *
     * @param f_packet The packe to be send.
     *
     * @param f_user_id The user ID of the client.
     */
    void sendAreaPacketClient(AOPacket *f_packet, int f_user_id);

    /**
     * @brief userJoinedArea Signals that a new client has joined an area.
     *
     * @details This is mostly a signal for more compelex features where multiple managers need to know of the change.
     *
     * @param f_area_index Area Index that the client joined in.
     *
     *
     * @param f_user_id The user ID of the client.
     */
    void userJoinedArea(int f_area_index, int f_user_id);

  private:
    /**
     * @brief The list of timers available in the area.
     */
    QList<QTimer *> m_timers;

    /**
     * @brief The user-facing and internal name of the area.
     */
    QString m_name;

    /**
     * @brief The index of the area in the server's area list.
     */
    int m_index;

    /**
     * @brief Pointer to the global music manager.
     */
    MusicManager *m_music_manager;

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
     * @brief The message of the area.
     *
     * @details The area message has multiple purposes. It can be used to provide general information for
     * RP or guidance for players joining the area. Unlike document it can be sent on area join. Like a MOTD, but for the area.
     */
    QString m_area_message;

    /**
     * @brief The Confidence Gauge's value for the Defence side.
     *
     * @details Unit is 10%, and the values range from 0 (= 0%) to 10 (= 100%).
     */
    int m_defHP;

    /**
     * @brief The Confidence Gauge's value for the Prosecutor side.
     *
     * @copydetails #m_defHP
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
    QString m_musicPlayedBy;

    /**
     * @brief A pointer to a Logger, used to send requests to log data.
     */
    Logger *m_logger;

    /**
     * @brief The evidence mod of the area.
     *
     * @see EvidenceMod
     */
    EvidenceMod m_eviMod;

    /**
     * @brief The list of notecards in the area.
     *
     * @details Notecards are plain text messages that can be left secretly in areas.
     * They can later be revealed all at once with a command call.
     *
     * Notecards have a `name: message` format, with the `name` being the recorder client's character's
     * charname at the time of recording, and `message` being a custom plain text message.
     */
    QMap<QString, QString> m_notecards;

    /**
     * @brief The state of the testimony recording / playback in the area.
     */
    TestimonyRecording m_testimonyRecording;

    QVector<QStringList> m_testimony; //!< Vector of all statements saved. Index 0 is always the title of the testimony.
    int m_statement;                  //!< Keeps track of the currently played statement.

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

    /**
     * @brief Whether or not to ignore the server defined background list. If true, any background can be set in an area.
     */
    bool m_ignoreBgList;

    /**
     * @brief Whether or not the area message is sent upon area join.
     */
    bool m_send_area_message;

    /**
     * @brief Collection of joined IDs to this area.
     */
    QVector<int> m_joined_ids;

    // Jukebox specific members
    /**
     * @brief Stores the songs added to the jukebox to be played.
     *
     * @details This contains the names of each song, noteworthy is that none of the songs are able to be entered twice.
     *
     */
    QVector<QString> m_jukebox_queue;

    /**
     * @brief Triggers the playing of the next song once the last song has fully played.
     *
     * @details While this may be considered bad design, I do not care.
     *          It triggers a direct broadcast of the MC packet in the area.
     */
    QTimer *m_jukebox_timer;

    /**
     * @brief Wether or not the jukebox is enabled in this area.
     */
    bool m_jukebox;

    /**
     * @brief Timer until the next IC message can be sent.
     */
    QTimer *m_message_floodguard_timer;

    /**
     * @brief If false, IC messages will be rejected.
     */
    bool m_can_send_ic_messages = true;

    /**
     * @brief If false, WTCE will be rejected.
     */
    bool m_can_send_wtce = true;

    /**
     * @brief If false, shouts are stripped from all messages in the area.
     */
    bool m_can_use_shouts = true;

  private slots:
    /**
     * @brief Allow game messages to be broadcasted.
     */
    void allowMessage();
};

#endif // AREA_DATA_H
