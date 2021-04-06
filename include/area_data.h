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
     * @brief The list of timers available in the area.
     */
    QList<QTimer*> timers;

    /**
     * @brief The user-facing and internal name of the area.
     */
    QString name;

    /**
     * @brief The index of the area in the server's area list.
     */
    int index;

    /**
     * @brief A list of the character IDs of all characters taken.
     */
    QList<int> characters_taken;

    /**
     * @brief A list of Evidence currently available in the area's court record.
     *
     * @details This contains *all* evidence, not just the ones a given side can see.
     *
     * @see HIDDEN_CM
     */
    QList<Evidence> evidence;

    /**
     * @brief The amount of clients inside the area.
     */
    int player_count;

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
     * @brief The status of the area.
     *
     * @see Status
     */
    Status status;

    /**
     * @brief The IDs of all the owners (or Case Makers / CMs) of the area.
     */
    QList<int> owners;

    /**
     * @brief The list of clients invited to the area.
     *
     * @see LOCKED and SPECTATABLE for the benefits of being invited.
     */
    QList<int> invited;

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
     * @brief The status of the area's accessibility to clients.
     *
     * @see LockStatus
     */
    LockStatus locked;

    /**
     * @brief The background of the area.
     *
     * @details Represents a directory's name in `base/background/` clientside.
     */
    QString background;

    /**
     * @brief If true, nobody may become the CM of this area.
     */
    bool is_protected;

    /**
     * @brief If true, clients are allowed to put on "shownames", custom names
     * in place of their character's normally displayed name.
     */
    bool showname_allowed;

    /**
     * @brief If true, clients are allowed to use the cursed art of iniswapping in the area.
     */
    bool iniswap_allowed;

    /**
     * @brief If true, the background of the area cannot be changed except by a moderator.
     */
    bool bg_locked;

    /**
     * @brief The hyperlink to the document of the area.
     *
     * @details Documents are generally used for cases or roleplays, where they contain the related game's
     * rules. #document can also be something like "None" if there is no case or roleplay being run.
     */
    QString document;

    /**
     * @brief The Confidence Gauge's value for the Defence side.
     *
     * @details Unit is 10%, and the values range from 0 (= 0%) to 10 (= 100%).
     */
    int def_hp;

    /**
     * @brief The Confidence Gauge's value for the Prosecutor side.
     *
     * @copydetails #def_hp
     */
    int pro_hp;

    /**
     * @brief The title of the music currently being played in the area.
     *
     * @details Title is a path to the music file, with the starting point on
     * `base/sounds/music/` clientside, with file extension.
     */
    QString current_music;

    /**
     * @brief The name of the client (or client's character) that started the currently playing music.
     */
    QString music_played_by;

    /**
     * @brief A pointer to a Logger, used to send requests to log data.
     */
    Logger* logger;

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
     * @brief The evidence mod of the area.
     *
     * @see EvidenceMod
     */
    EvidenceMod evi_mod;
    QMap<QString, QString> notecards;

    /**
     * @brief The three "states" the testimony recording system can have in an area.
     */
    enum TestimonyRecording{
        STOPPED,
        RECORDING,
        UPDATE,
        AMEND,
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
     * @var TestimonyRecording AMEND
     * The testimony recorder is active and inserts the next message after the currently displayed ic-message
     * This will increase the size by 1.
     */

    /**
     * @var TestimonyRecording PLAYBACK
     * The testimony recorder is inactive and ic-messages in the buffer will be played back.
     */

    /// Exposes the metadata of the TestimonyRecording enum.
    Q_ENUM(TestimonyRecording);
    TestimonyRecording test_rec;

    QVector<QStringList> testimony;

};

#endif // AREA_DATA_H
