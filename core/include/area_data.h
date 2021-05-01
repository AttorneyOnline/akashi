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

#include "include/logger.h"

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
     * @brief A client in the area has left the area.
     *
     * @details This function counts down the playercount and removes the character from the list of taken characters.
     *
     * @param f_charId The character ID of the area.
     */
    void clientLeftArea(int f_charId);

    QList<int> owners() const;

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
     */
    bool removeOwner(int f_clientId);

    bool blankpostingAllowed() const;

    bool isProtected() const;

    LockStatus lockStatus() const;

    /**
     * @brief invite
     * @param f_clientId
     * @return True if the client was successfully invited. False if they were already in the list of invited people.
     */
    bool invite(int f_clientId);

    int playerCount() const;

    QList<QTimer *> timers() const;

    QString name() const;

    int index() const;

    QList<int> charactersTaken() const;

    QList<Evidence> evidence() const;

    Status status() const;

    QList<int> invited() const;

    LockStatus locked() const;

    QString background() const;

    bool shownameAllowed() const;

    bool iniswapAllowed() const;

    bool bgLocked() const;

    QString document() const;

    int defHP() const;

    int proHP() const;

    QString currentMusic() const;

    QString musicPlayerBy() const;

    Logger *logger() const;

    EvidenceMod eviMod() const;

    QMap<QString, QString> notecards() const;

    TestimonyRecording testimonyRecording() const;

    QVector<QStringList> testimony() const;

    int statement() const;

    QStringList judgelog() const;

    QStringList lastICMessage() const;

    bool forceImmediate() const;

    bool toggleMusic() const;

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
