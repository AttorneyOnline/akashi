// AI-generated: written by Claude.
// A headless AO2 client built on the real client's netcode. It joins a
// running akashi server the same way the desktop client does, then walks
// the playtest checklist: pick a character, chat in OOC and IC, change
// the music, throw a judge splash, present evidence with every escape
// character in it, keepalive, get denied a mod command, fail a login, log
// in with the modpass and run a database-backed mod command; then, as a
// moderator: set a judge penalty, call a mod, announce a case, and end
// the session by kicking itself through the MA packet. Exits 0 when
// every step got the expected answer, 1 on a timeout or failure.
//
// The "evidence" and "testimony" modes instead play multi-client scenes:
// the hidden-evidence numbering dance and the testimony record/playback/
// save/load round trip.
// Usage: ao2_miniclient [address] [port] [modpass] [classic|evidence|testimony]
#include "aopacket.h"
#include "websocketconnection.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSet>
#include <QTimer>

#include <iostream>

class MiniClient : public QObject
{
    Q_OBJECT

  public:
    MiniClient(const QString &f_address, int f_port, const QString &f_modpass, QObject *parent = nullptr) :
        QObject(parent),
        m_modpass(f_modpass)
    {
        m_connection = new WebSocketConnection(this);
        m_watchdog = new QTimer(this);
        m_watchdog->setSingleShot(true);
        connect(m_watchdog, &QTimer::timeout, this, [this] { fail("timed out waiting for: " + m_waiting_for); });

        // Sends are paced like a human client, or the burst of a full run
        // trips the server's packets-per-second limit.
        m_pacer = new QTimer(this);
        connect(m_pacer, &QTimer::timeout, this, [this] {
            if (m_outbox.isEmpty()) {
                m_pacer->stop();
                return;
            }
            AOPacket l_packet = m_outbox.takeFirst();
            say("C: " + trim(l_packet.toString()));
            m_connection->sendPacket(l_packet);
        });
        connect(m_connection, &WebSocketConnection::receivedPacket, this, &MiniClient::handlePacket);
        connect(m_connection, &WebSocketConnection::errorOccurred, this, [this](QString error) { fail("connection error: " + error); });
        connect(m_connection, &WebSocketConnection::disconnectedFromServer, this, [this] {
            if (!m_done) {
                fail("server closed the connection early");
            }
        });

        say("connecting to " + f_address + ":" + QString::number(f_port));
        step(Step::Connect, "decryptor");
        m_connection->connectToServer(f_address, f_port);
    }

  private Q_SLOTS:
    // Each step only accepts the one packet it waits for; everything else
    // (ARUP, FM, ambience MC, player list updates) is unsolicited traffic.
    void handlePacket(AOPacket f_packet)
    {
        const QString l_header = f_packet.header();
        const QStringList l_content = f_packet.content();
        say("S: " + trim(f_packet.toString()));

        switch (m_step) {
        case Step::Connect:
            // The handshake reactions mirror the real client's packet_distribution.cpp.
            if (l_header == "decryptor") {
                step(Step::Identify, "ID");
                send(AOPacket("HI", {m_hdid}));
            }
            break;
        case Step::Identify:
            if (l_header == "ID") {
                m_client_id = l_content.at(0).toInt();
                send(AOPacket("ID", {"AO2", "2.11.0"}));
                step(Step::Counts, "SI");
                send(AOPacket("askchaa"));
            }
            break;
        case Step::Counts:
            if (l_header == "SI") {
                step(Step::Characters, "SC");
                send(AOPacket("RC"));
            }
            break;
        case Step::Characters:
            if (l_header == "SC") {
                m_characters = l_content;
                step(Step::Music, "SM");
                send(AOPacket("RM"));
            }
            break;
        case Step::Music:
            if (l_header == "SM") {
                // Split the joined area+music list the way the client does: the
                // first entry with an audio extension starts the music half.
                for (const QString &l_entry : l_content) {
                    if (!m_songs.isEmpty() || l_entry.endsWith(".wav") || l_entry.endsWith(".mp3") || l_entry.endsWith(".mp4") || l_entry.endsWith(".ogg") || l_entry.endsWith(".opus")) {
                        m_songs.append(l_entry);
                    }
                }
                step(Step::Join, "DONE");
                send(AOPacket("RD"));
                send(AOPacket("CT", {"miniclient", ""}));
            }
            break;
        case Step::Join:
            if (l_header == "DONE") {
                m_char_id = m_characters.indexOf("Phoenix");
                if (m_char_id < 0) {
                    fail("the server has no Phoenix to pick");
                    return;
                }
                step(Step::CharacterSelect, "PV");
                send(AOPacket("CC", {QString::number(m_client_id), QString::number(m_char_id), m_hdid}));
            }
            break;
        case Step::CharacterSelect:
            if (l_header == "PV") {
                if (l_content.at(2).toInt() != m_char_id) {
                    fail("PV confirmed the wrong character: " + l_content.at(2));
                    return;
                }
                step(Step::OocChat, "CT echo");
                send(AOPacket("CT", {"miniclient", "Hello from the mini client!"}));
            }
            break;
        case Step::OocChat:
            if (l_header == "CT" && l_content.value(1) == "Hello from the mini client!") {
                step(Step::IcChat, "MS echo");
                send(icMessage("Hello court, mini client speaking!"));
            }
            break;
        case Step::IcChat:
            if (l_header == "MS" && icEchoText(l_content) == "Hello court, mini client speaking!") {
                m_requested_song = m_songs.value(1);
                step(Step::MusicChange, "MC echo");
                send(AOPacket("MC", {m_requested_song, QString::number(m_char_id), "", "0"}));
            }
            break;
        case Step::MusicChange:
            if (l_header == "MC" && l_content.value(0) == m_requested_song) {
                step(Step::JudgeSplash, "RT echo");
                send(AOPacket("RT", {"testimony1"}));
            }
            break;
        case Step::JudgeSplash:
            if (l_header == "RT" && l_content.value(0) == "testimony1") {
                // Move to Courtroom 1, where the evidence mod allows adding.
                step(Step::AreaMove, "area move confirmation");
                send(AOPacket("MC", {"Courtroom 1", QString::number(m_char_id)}));
            }
            break;
        case Step::AreaMove:
            if (l_header == "CT" && l_content.value(1).contains("You moved to area Courtroom 1")) {
                step(Step::Evidence, "LE with the new evidence");
                send(AOPacket("PE", {"Cross & Sword", "It has a # and a % on it.", "sword&shield.png"}));
            }
            break;
        case Step::Evidence:
            // The connection decodes fields on receipt, so the packed
            // evidence field reads with its escape codes resolved.
            if (l_header == "LE" && l_content.contains("Cross & Sword&It has a # and a % on it.&sword&shield.png")) {
                step(Step::Keepalive, "CHECK");
                send(AOPacket("CH", {QString::number(m_char_id)}));
            }
            break;
        case Step::Keepalive:
            if (l_header == "CHECK") {
                step(Step::ModCommandDenied, "permission denial");
                send(AOPacket("CT", {"miniclient", "/baninfo 1"}));
            }
            break;
        case Step::ModCommandDenied:
            if (l_header == "CT" && l_content.value(1).contains("You do not have permission")) {
                step(Step::LoginPrompt, "login prompt");
                send(AOPacket("CT", {"miniclient", "/login"}));
            }
            break;
        case Step::LoginPrompt:
            if (l_header == "CT" && l_content.value(1).contains("Entering login prompt")) {
                step(Step::LoginWrong, "AUTH 0");
                send(AOPacket("CT", {"miniclient", "not-the-modpass"}));
            }
            break;
        case Step::LoginWrong:
            if (l_header == "AUTH" && l_content.value(0) == "0") {
                step(Step::LoginPromptAgain, "login prompt");
                send(AOPacket("CT", {"miniclient", "/login"}));
            }
            break;
        case Step::LoginPromptAgain:
            if (l_header == "CT" && l_content.value(1).contains("Entering login prompt")) {
                step(Step::LoginRight, "AUTH 1");
                send(AOPacket("CT", {"miniclient", m_modpass}));
            }
            break;
        case Step::LoginRight:
            if (l_header == "AUTH" && l_content.value(0) == "1") {
                step(Step::DatabaseQuery, "ban info");
                send(AOPacket("CT", {"miniclient", "/baninfo 1"}));
            }
            break;
        case Step::DatabaseQuery:
            if (l_header == "CT" && l_content.value(1).contains("Ban Info for 1")) {
                step(Step::JudgePenalty, "HP echo");
                send(AOPacket("HP", {"1", "7"}));
            }
            break;
        case Step::JudgePenalty:
            if (l_header == "HP" && l_content.value(0) == "1" && l_content.value(1) == "7") {
                step(Step::Modcall, "modcall notice");
                send(AOPacket("ZZ", {"Testing the modcall", "-1"}));
            }
            break;
        case Step::Modcall:
            // Logged in as a moderator, so our own call comes right back.
            if (l_header == "ZZ" && l_content.value(0).contains("!!!MODCALL!!!")) {
                step(Step::CaseAlert, "case announcement");
                send(AOPacket("CASEA", {"Test Case", "1", "0", "0", "0", "0"}));
            }
            break;
        case Step::CaseAlert:
            if (l_header == "CASEA" && l_content.value(0).contains("=== Case Announcement ===")) {
                step(Step::AreaOwnership, "ARUP CM update");
                send(AOPacket("CT", {"miniclient", "/cm"}));
            }
            break;
        case Step::AreaOwnership:
            // Becoming CM must show up in the CM area update as "[id] char".
            if (l_header == "ARUP" && l_content.value(0) == "2" && l_content.join(",").contains("[" + QString::number(m_client_id) + "]")) {
                step(Step::AreaStatus, "ARUP status update");
                send(AOPacket("CT", {"miniclient", "/status casing"}));
            }
            break;
        case Step::AreaStatus:
            if (l_header == "ARUP" && l_content.value(0) == "1" && l_content.contains("CASING")) {
                step(Step::AreaLock, "ARUP lock update");
                send(AOPacket("CT", {"miniclient", "/area_lock"}));
            }
            break;
        case Step::AreaLock:
            if (l_header == "ARUP" && l_content.value(0) == "3" && l_content.contains("LOCKED")) {
                step(Step::AreaUnlock, "ARUP unlock update");
                send(AOPacket("CT", {"miniclient", "/area_unlock"}));
            }
            break;
        case Step::AreaUnlock:
            if (l_header == "ARUP" && l_content.value(0) == "3" && !l_content.join(",").contains("LOCKED")) {
                step(Step::AreaUncm, "ARUP CM cleared");
                send(AOPacket("CT", {"miniclient", "/uncm"}));
            }
            break;
        case Step::AreaUncm:
            if (l_header == "ARUP" && l_content.value(0) == "2" && !l_content.join(",").contains("[" + QString::number(m_client_id) + "]")) {
                step(Step::SelfKick, "KK");
                send(AOPacket("MA", {QString::number(m_client_id), "0", "kicked by the playtest"}));
            }
            break;
        case Step::SelfKick:
            if (l_header == "KK") {
                m_done = true;
                m_watchdog->stop();
                say("every step answered, kicked ourselves goodbye");
                QTimer::singleShot(500, qApp, [] { qApp->exit(0); });
            }
            break;
        }
    }

  private:
    enum class Step
    {
        Connect,
        Identify,
        Counts,
        Characters,
        Music,
        Join,
        CharacterSelect,
        OocChat,
        IcChat,
        MusicChange,
        JudgeSplash,
        AreaMove,
        Evidence,
        Keepalive,
        ModCommandDenied,
        LoginPrompt,
        LoginWrong,
        LoginPromptAgain,
        LoginRight,
        DatabaseQuery,
        JudgePenalty,
        Modcall,
        CaseAlert,
        AreaOwnership,
        AreaStatus,
        AreaLock,
        AreaUnlock,
        AreaUncm,
        SelfKick,
    };

    void send(AOPacket f_packet)
    {
        m_outbox.append(f_packet);
        if (!m_pacer->isActive()) {
            m_pacer->start(150);
        }
    }

    AOPacket icMessage(const QString &f_text) const
    {
        return AOPacket("MS", {"chat", "-", "Phoenix", "normal", f_text, "def", "1", "0", QString::number(m_char_id), "0", "0", "0", "0", "0", "0"});
    }

    // Reads the message text back out of an MS echo.
    QString icEchoText(const QStringList &f_content) const
    {
        return f_content.value(4);
    }

    void step(Step f_step, const QString &f_waiting_for)
    {
        m_step = f_step;
        m_waiting_for = f_waiting_for;
        say("--- waiting for " + f_waiting_for + " ---");
        m_watchdog->start(7000);
    }

    void say(const QString &f_text)
    {
        std::cout << f_text.toStdString() << std::endl;
    }

    void fail(const QString &f_reason)
    {
        say("FAILED: " + f_reason);
        qApp->exit(1);
    }

    static QString trim(QString f_text)
    {
        if (f_text.size() > 120) {
            f_text = f_text.left(120) + "…";
        }
        return f_text;
    }

    WebSocketConnection *m_connection;
    QTimer *m_watchdog;
    QTimer *m_pacer;
    QList<AOPacket> m_outbox;
    Step m_step = Step::Connect;
    QString m_waiting_for;
    QString m_hdid = "miniclient-hdid";
    QString m_modpass;
    QString m_requested_song;
    int m_client_id = -1;
    int m_char_id = -1;
    QStringList m_characters;
    QStringList m_songs;
    bool m_done = false;
};

// One scripted participant in a multi-client scene. It joins the way the
// desktop client does (the same choreography as the single-client run),
// then the scene drives it by sending packets and reads what it saw.
class DanceClient : public QObject
{
    Q_OBJECT

  public:
    DanceClient(const QString &f_name, const QString &f_character, const QString &f_address, int f_port, int f_pick_offset = 0, QObject *parent = nullptr) :
        QObject(parent),
        m_name(f_name),
        m_character(f_character),
        m_pick_offset(f_pick_offset)
    {
        m_connection = new WebSocketConnection(this);
        m_pacer = new QTimer(this);
        connect(m_pacer, &QTimer::timeout, this, [this] {
            if (m_outbox.isEmpty()) {
                m_pacer->stop();
                return;
            }
            AOPacket l_packet = m_outbox.takeFirst();
            say("C: " + l_packet.toString().left(120));
            m_connection->sendPacket(l_packet);
        });
        connect(m_connection, &WebSocketConnection::receivedPacket, this, &DanceClient::handlePacket);
        connect(m_connection, &WebSocketConnection::errorOccurred, this, [this](QString error) { Q_EMIT failed(m_name + " connection error: " + error); });
        m_connection->connectToServer(f_address, f_port);
    }

    void send(AOPacket f_packet)
    {
        m_outbox.append(f_packet);
        if (!m_pacer->isActive()) {
            m_pacer->start(150);
        }
    }

    void sendOoc(const QString &f_message) { send(AOPacket("CT", {m_name, f_message})); }

    int charId() const { return m_char_id; }
    QString character() const { return m_character; }
    QStringList evidenceNames() const { return m_evidence_names; }

  Q_SIGNALS:
    void ready();
    void oocReceived(const QString &f_message);
    void icReceived(const QStringList &f_fields);
    void rtReceived(const QString &f_kind);
    void mcReceived(const QStringList &f_fields);
    void evidenceChanged(const QStringList &f_names);
    void loggedIn();
    void failed(const QString &f_reason);

  private Q_SLOTS:
    void handlePacket(AOPacket f_packet)
    {
        const QString l_header = f_packet.header();
        const QStringList l_content = f_packet.content();

        // The join choreography, mirroring the single-client run.
        if (l_header == "decryptor") {
            send(AOPacket("HI", {m_name + "-hdid"}));
        }
        else if (l_header == "ID" && m_client_id < 0) {
            m_client_id = l_content.value(0).toInt();
            send(AOPacket("ID", {"AO2", "2.11.0"}));
            send(AOPacket("askchaa"));
        }
        else if (l_header == "SI") {
            send(AOPacket("RC"));
        }
        else if (l_header == "SC") {
            m_characters = l_content;
            send(AOPacket("RM"));
        }
        else if (l_header == "SM") {
            send(AOPacket("RD"));
        }
        else if (l_header == "DONE" && m_char_id < 0) {
            // An empty character wish means "anyone but Phoenix", so the
            // scene works with whatever roster the server runs; the offset
            // lets several wishers pick different characters.
            if (m_character.isEmpty()) {
                int l_skip = m_pick_offset;
                for (const QString &l_candidate : m_characters) {
                    if (l_candidate != "Phoenix" && !l_candidate.isEmpty() && l_skip-- == 0) {
                        m_character = l_candidate;
                        break;
                    }
                }
            }
            m_char_id = m_characters.indexOf(m_character);
            if (m_char_id < 0) {
                Q_EMIT failed(m_name + ": the server has no " + m_character);
                return;
            }
            send(AOPacket("CC", {QString::number(m_client_id), QString::number(m_char_id), m_name + "-hdid"}));
        }
        else if (l_header == "PV") {
            Q_EMIT ready();
        }
        else if (l_header == "LE") {
            // The list arrives whole, one name&description&image entry per
            // field, exactly how the desktop client reads it.
            m_evidence_names.clear();
            for (const QString &l_field : l_content) {
                m_evidence_names.append(l_field.split("&").value(0));
            }
            say("sees evidence: [" + m_evidence_names.join(", ") + "]");
            Q_EMIT evidenceChanged(m_evidence_names);
        }
        else if (l_header == "CT") {
            Q_EMIT oocReceived(l_content.value(1));
        }
        else if (l_header == "MS") {
            Q_EMIT icReceived(l_content);
        }
        else if (l_header == "RT") {
            Q_EMIT rtReceived(l_content.value(0));
        }
        else if (l_header == "MC") {
            Q_EMIT mcReceived(l_content);
        }
        else if (l_header == "AUTH" && l_content.value(0) == "1") {
            Q_EMIT loggedIn();
        }
    }

  private:
    void say(const QString &f_text)
    {
        std::cout << m_name.toStdString() << " " << f_text.toStdString() << std::endl;
    }

    QString m_name;
    QString m_character;
    WebSocketConnection *m_connection;
    QTimer *m_pacer;
    QList<AOPacket> m_outbox;
    QStringList m_characters;
    QStringList m_evidence_names;
    int m_client_id = -1;
    int m_char_id = -1;
    int m_pick_offset = 0;
};

// The hidden-evidence numbering scene, played by three clients against a
// live server. In hidden mode each side numbers a different subset of the
// court record, and the desktop client presents "position + 1" in its own
// list while every receiver looks the number up in THEIRS - so the server
// must renumber per viewer, the tsu3 legacy this proves end to end:
//   1. the moderator turns on hidden mode and files a prosecution secret,
//      a defense secret and a public item, in that order;
//   2. the defense must see two items and the prosecution two OTHER items -
//      neither side's secret may ever leak to the other side beforehand;
//   3. the defense presents ITS number 1 (the defense secret) - presenting
//      reveals a hidden item, so the number each viewer receives must name
//      the SAME item in their own list: 1 for the defense, 2 for the
//      moderator, and 2 for the prosecution, whose list just gained the
//      revealed secret;
//   4. the moderator deletes ITS number 1 (the prosecution secret) - all
//      three must end up seeing exactly the defense secret and the public
//      item.
class EvidenceDance : public QObject
{
    Q_OBJECT

  public:
    EvidenceDance(const QString &f_address, int f_port, const QString &f_modpass, QObject *parent = nullptr) :
        QObject(parent),
        m_address(f_address),
        m_port(f_port),
        m_modpass(f_modpass)
    {
        m_watchdog = new QTimer(this);
        m_watchdog->setSingleShot(true);
        connect(m_watchdog, &QTimer::timeout, this, [this] { fail("timed out waiting for: " + m_waiting_for); });

        phase(Phase::CmLogin, "the moderator to join and log in");
        m_cm = new DanceClient("cm", "Phoenix", m_address, m_port, 0, this);
        wireClient(m_cm);

        connect(m_cm, &DanceClient::ready, this, [this] { m_cm->sendOoc("/login"); });
        connect(m_cm, &DanceClient::oocReceived, this, [this](const QString &f_message) {
            if (f_message.contains("Entering login prompt")) {
                m_cm->sendOoc(m_modpass);
            }
            else if (f_message.contains("Changed evidence mod.") && m_phase == Phase::CmLogin) {
                phase(Phase::DefJoin, "the defense to join");
                m_def = new DanceClient("def", "", m_address, m_port, 0, this);
                wireClient(m_def);
                connect(m_def, &DanceClient::ready, this, [this] {
                    phase(Phase::DefPos, "the defense's list after /pos def");
                    m_def->sendOoc("/pos def");
                });
                connect(m_def, &DanceClient::evidenceChanged, this, [this] { onListsChanged("def"); });
                connect(m_def, &DanceClient::icReceived, this, [this](const QStringList &f_fields) { onIc("def", f_fields); });
            }
        });
        connect(m_cm, &DanceClient::loggedIn, this, [this] { m_cm->sendOoc("/evidence_mod hiddencm"); });
        connect(m_cm, &DanceClient::evidenceChanged, this, [this] { onListsChanged("cm"); });
        connect(m_cm, &DanceClient::icReceived, this, [this](const QStringList &f_fields) { onIc("cm", f_fields); });
    }

  private:
    enum class Phase
    {
        CmLogin,
        DefJoin,
        DefPos,
        ProJoin,
        ProPos,
        Adding,
        Presenting,
        Deleting,
    };

    void wireClient(DanceClient *f_client)
    {
        connect(f_client, &DanceClient::failed, this, [this](const QString &f_reason) { fail(f_reason); });
    }

    void onListsChanged(const QString &f_who)
    {
        const QStringList l_cm_list = m_cm->evidenceNames();
        const QStringList l_def_list = m_def ? m_def->evidenceNames() : QStringList();
        const QStringList l_pro_list = m_pro ? m_pro->evidenceNames() : QStringList();

        // The prosecution's secret must never reach the defense's list, and
        // the defense's secret stays hidden from the prosecution until the
        // moment it is presented.
        if (l_def_list.contains("ProSecret")) {
            fail("a hidden item leaked into the defense's list");
            return;
        }
        if (l_pro_list.contains("DefSecret") && m_phase != Phase::Presenting && m_phase != Phase::Deleting) {
            fail("the defense's secret leaked to the prosecution before it was presented");
            return;
        }

        switch (m_phase) {
        case Phase::DefPos:
            if (f_who == "def") {
                // The /pos command answers with a fresh, still empty list.
                phase(Phase::ProJoin, "the prosecution to join");
                m_pro = new DanceClient("pro", "", m_address, m_port, 1, this);
                wireClient(m_pro);
                connect(m_pro, &DanceClient::ready, this, [this] {
                    phase(Phase::ProPos, "the prosecution's list after /pos pro");
                    m_pro->sendOoc("/pos pro");
                });
                connect(m_pro, &DanceClient::evidenceChanged, this, [this] { onListsChanged("pro"); });
                connect(m_pro, &DanceClient::icReceived, this, [this](const QStringList &f_fields) { onIc("pro", f_fields); });
            }
            break;
        case Phase::ProPos:
            if (f_who == "pro") {
                phase(Phase::Adding, "every list after the moderator files three items");
                m_cm->send(AOPacket("PE", {"ProSecret", "<owner=pro>A prosecution secret.", "pro.png"}));
                m_cm->send(AOPacket("PE", {"DefSecret", "<owner=def>A defense secret.", "def.png"}));
                m_cm->send(AOPacket("PE", {"Public", "Everyone sees this.", "pub.png"}));
            }
            break;
        case Phase::Adding:
            if (l_cm_list == QStringList{"ProSecret", "DefSecret", "Public"} && l_def_list == QStringList{"DefSecret", "Public"} && l_pro_list == QStringList{"ProSecret", "Public"}) {
                say("each side numbers its own list: three items for the moderator, two each for the sides");
                phase(Phase::Presenting, "the presented item to reach every viewer under their own number");
                // The desktop client presents its 0-based list position + 1.
                m_def->send(AOPacket("MS", {"chat", "-", m_def->character(), "normal", "Take this!", "def", "1", "0", QString::number(m_def->charId()), "0", "0", "1", "0", "0", "0"}));
            }
            break;
        case Phase::Deleting:
            if (!m_done && l_cm_list == QStringList{"DefSecret", "Public"} && l_def_list == QStringList{"DefSecret", "Public"} && l_pro_list == QStringList{"DefSecret", "Public"}) {
                m_done = true;
                say("the moderator deleted ITS number 1 and the right item vanished for everyone");
                say("the whole dance played out: filtering, per-viewer numbers, the reveal and deletion all line up");
                m_watchdog->stop();
                QTimer::singleShot(500, qApp, [] { qApp->exit(0); });
            }
            break;
        default:
            break;
        }
    }

    void onIc(const QString &f_who, const QStringList &f_fields)
    {
        if (m_phase != Phase::Presenting || f_fields.value(4) != "Take this!") {
            return;
        }

        // Every viewer's number must name the SAME item in their own list -
        // that is what "both sides see the presented evidence" means on a
        // protocol whose numbers are per-viewer list positions.
        const QString l_number = f_fields.value(11);
        DanceClient *l_viewer = f_who == "def" ? m_def : (f_who == "cm" ? m_cm : m_pro);
        if (l_viewer->evidenceNames().value(l_number.toInt() - 1) != "DefSecret") {
            fail(f_who + "'s number " + l_number + " does not name the presented item in their list");
            return;
        }
        const QString l_expected = f_who == "def" ? "1" : "2";
        if (l_number != l_expected) {
            fail(f_who + " got evidence number " + l_number + ", expected " + l_expected);
            return;
        }
        m_saw_present.insert(f_who);

        if (m_saw_present.size() == 3) {
            // The reveal is what lets the prosecution resolve the number at
            // all: its list must have gained the presented secret.
            if (m_pro->evidenceNames() != QStringList{"ProSecret", "DefSecret", "Public"}) {
                fail("presenting did not reveal the item to the prosecution");
                return;
            }
            say("one presentation, three viewers: numbers 1, 2 and 2 all name the defense secret");
            phase(Phase::Deleting, "every list after the moderator deletes its number 1");
            // The desktop client deletes by 0-based position in its list.
            m_cm->send(AOPacket("DE", {"0"}));
        }
    }

    void phase(Phase f_phase, const QString &f_waiting_for)
    {
        m_phase = f_phase;
        m_waiting_for = f_waiting_for;
        say("--- waiting for " + f_waiting_for + " ---");
        m_watchdog->start(15000);
    }

    void say(const QString &f_text)
    {
        std::cout << f_text.toStdString() << std::endl;
    }

    void fail(const QString &f_reason)
    {
        say("FAILED: " + f_reason);
        qApp->exit(1);
    }

    QString m_address;
    int m_port;
    QString m_modpass;
    QTimer *m_watchdog;
    QString m_waiting_for;
    DanceClient *m_cm = nullptr;
    DanceClient *m_def = nullptr;
    DanceClient *m_pro = nullptr;
    Phase m_phase = Phase::CmLogin;
    QSet<QString> m_saw_present;
    bool m_done = false;
};

// The testimony scene, played by two clients against a live server: a
// leader records a testimony, plays it back, edits it and takes it through
// a save/load round trip; a witness proves what the room really sees.
// The load-bearing assertions:
//   1. a LIVE recorded message reaches the room with the speaker's own
//      char id, but every REPLAYED statement arrives with char id -1 -
//      a replayed id would read as some current player's own message and
//      wipe that player's input panel mid-typing;
//   2. navigation (>, <, =, >n) replays the right statements, loops past
//      the end and stays at the first;
//   3. /update and /add change the right spots, and a statement holding
//      the protocol's own separator characters (# and %) survives the
//      save file round trip, because /loadtestimony parses every line
//      through the same IC message parsing a live packet gets.
class TestimonyDance : public QObject
{
    Q_OBJECT

  public:
    TestimonyDance(const QString &f_address, int f_port, const QString &f_modpass, QObject *parent = nullptr) :
        QObject(parent),
        m_address(f_address),
        m_port(f_port),
        m_modpass(f_modpass)
    {
        m_save_name = "minitest-" + QString::number(QDateTime::currentMSecsSinceEpoch());
        m_watchdog = new QTimer(this);
        m_watchdog->setSingleShot(true);
        connect(m_watchdog, &QTimer::timeout, this, [this] { fail("timed out waiting for: " + m_waiting_for); });

        phase(Phase::LeaderLogin, "the leader to join and log in");
        m_leader = new DanceClient("leader", "Phoenix", m_address, m_port, 0, this);
        connect(m_leader, &DanceClient::failed, this, [this](const QString &f_reason) { fail(f_reason); });
        connect(m_leader, &DanceClient::ready, this, [this] { m_leader->sendOoc("/login"); });
        connect(m_leader, &DanceClient::oocReceived, this, &TestimonyDance::onLeaderOoc);
        connect(m_leader, &DanceClient::loggedIn, this, [this] {
            phase(Phase::WitnessJoin, "the witness to join");
            m_witness = new DanceClient("witness", "", m_address, m_port, 0, this);
            connect(m_witness, &DanceClient::failed, this, [this](const QString &f_reason) { fail(f_reason); });
            connect(m_witness, &DanceClient::ready, this, [this] {
                phase(Phase::Testify, "the recording to start");
                m_leader->sendOoc("/testify");
            });
            connect(m_witness, &DanceClient::oocReceived, this, &TestimonyDance::onWitnessOoc);
            connect(m_witness, &DanceClient::icReceived, this, &TestimonyDance::onWitnessIc);
            connect(m_witness, &DanceClient::rtReceived, this, [this](const QString &f_kind) { m_last_rt = f_kind; });
        });
    }

  private:
    enum class Phase
    {
        LeaderLogin,
        WitnessJoin,
        Testify,
        RecordTitle,
        RecordFirst,
        RecordSecond,
        Examine,
        NextOne,
        NextTwo,
        LoopAround,
        StayAtFirst,
        JumpTwo,
        UpdatePrompt,
        UpdateSend,
        AddPrompt,
        AddSend,
        ListAll,
        Save,
        DeleteCurrent,
        ListAfterDelete,
        Load,
        ListAfterLoad,
        ExamineLoaded,
        NavigateLoaded,
    };

    AOPacket icMessage(const QString &f_text) const
    {
        return AOPacket("MS", {"chat", "-", m_leader->character(), "normal", f_text, "def", "1", "0", QString::number(m_leader->charId()), "0", "0", "0", "0", "0", "0"});
    }

    // IC messages chain quickly here, so each waits out the area's message
    // floodguard first or the server drops it without a word.
    void sendIcSoon(const QString &f_text)
    {
        QTimer::singleShot(400, this, [this, f_text] { m_leader->send(icMessage(f_text)); });
    }

    void onLeaderOoc(const QString &f_message)
    {
        if (f_message.contains("Entering login prompt")) {
            m_leader->sendOoc(m_modpass);
            return;
        }

        switch (m_phase) {
        case Phase::Testify:
            if (f_message.contains("already in progress")) {
                // A previous run left the area recording; stop it and retry.
                m_leader->sendOoc("/pause");
                m_leader->sendOoc("/testify");
            }
            else if (f_message.contains("Started testimony recording")) {
                phase(Phase::RecordTitle, "the room to hear the title");
                sendIcSoon("The Turnabout");
            }
            break;
        case Phase::UpdatePrompt:
            if (f_message.contains("will replace the currently selected testimony line")) {
                phase(Phase::UpdateSend, "the replacement statement to reach the room");
                sendIcSoon("Second statement, revised");
            }
            break;
        case Phase::UpdateSend:
            if (f_message.contains("Updated current statement")) {
                m_update_confirmed = true;
                finishUpdateWhenBothArrived();
            }
            break;
        case Phase::AddPrompt:
            if (f_message.contains("will be inserted into the testimony")) {
                phase(Phase::AddSend, "the inserted statement to reach the room");
                sendIcSoon("Exhibit #1 at 100%");
            }
            break;
        case Phase::ListAll:
            if (f_message.contains("[1]First statement")) {
                if (!f_message.contains("[2]Second statement, revised") || !f_message.contains("[3]Exhibit #1 at 100%")) {
                    fail("the testimony listing is wrong: " + f_message);
                    return;
                }
                say("the listing shows the update and the insert in their spots");
                phase(Phase::Save, "the testimony to save");
                m_leader->sendOoc("/savetestimony " + m_save_name);
            }
            break;
        case Phase::Save:
            if (f_message.contains("Testimony saved")) {
                phase(Phase::DeleteCurrent, "the current statement to be deleted");
                m_leader->sendOoc("/delete");
            }
            break;
        case Phase::DeleteCurrent:
            if (f_message.contains("has been deleted from the testimony")) {
                if (!f_message.contains("id 2")) {
                    fail("the wrong statement was deleted: " + f_message);
                    return;
                }
                phase(Phase::ListAfterDelete, "the listing without the deleted statement");
                m_leader->sendOoc("/testimony");
            }
            break;
        case Phase::ListAfterDelete:
            if (f_message.contains("[1]First statement")) {
                if (f_message.contains("revised") || !f_message.contains("[2]Exhibit #1 at 100%")) {
                    fail("the deletion did not remove the right statement: " + f_message);
                    return;
                }
                phase(Phase::Load, "the saved testimony to load back");
                m_leader->sendOoc("/loadtestimony " + m_save_name);
            }
            break;
        case Phase::Load:
            if (f_message.contains("Testimony loaded successfully")) {
                phase(Phase::ListAfterLoad, "the listing of the loaded testimony");
                m_leader->sendOoc("/testimony");
            }
            break;
        case Phase::ListAfterLoad:
            if (f_message.contains("[1]First statement")) {
                if (!f_message.contains("[2]Second statement, revised") || !f_message.contains("[3]Exhibit #1 at 100%")) {
                    fail("the saved testimony did not survive the round trip: " + f_message);
                    return;
                }
                say("the save file round-tripped, separator characters and all");
                phase(Phase::ExamineLoaded, "the loaded title to replay to the room");
                m_leader->sendOoc("/examine");
            }
            break;
        default:
            break;
        }
    }

    void onWitnessOoc(const QString &f_message)
    {
        if (m_phase == Phase::LoopAround && f_message.contains("Looping to first statement")) {
            m_saw_loop_notice = true;
        }
    }

    void onWitnessIc(const QStringList &f_fields)
    {
        const QString l_text = f_fields.value(4);
        const QString l_char_id = f_fields.value(8);
        const QString l_leader_id = QString::number(m_leader->charId());

        switch (m_phase) {
        case Phase::RecordTitle:
            if (l_text == "~~-- The Turnabout --") {
                // A live message during recording still belongs to its speaker.
                if (l_char_id != l_leader_id || f_fields.value(14) != "3" || m_last_rt != "testimony1") {
                    fail("the title broadcast is wrong: char id " + l_char_id + ", color " + f_fields.value(14) + ", after RT " + m_last_rt);
                    return;
                }
                phase(Phase::RecordFirst, "the first statement to reach the room");
                sendIcSoon("First statement");
            }
            break;
        case Phase::RecordFirst:
            if (l_text == "First statement") {
                phase(Phase::RecordSecond, "the second statement to reach the room");
                sendIcSoon("Second statement");
            }
            break;
        case Phase::RecordSecond:
            if (l_text == "Second statement") {
                phase(Phase::Examine, "the examination to start with the title");
                m_leader->sendOoc("/pause");
                m_leader->sendOoc("/examine");
            }
            break;
        case Phase::Examine:
            if (l_text == "~~-- The Turnabout --") {
                // Replayed statements belong to nobody; the recorded id
                // would read as some current player's own message.
                if (l_char_id != "-1" || m_last_rt != "testimony2") {
                    fail("the replayed title is wrong: char id " + l_char_id + ", after RT " + m_last_rt);
                    return;
                }
                phase(Phase::NextOne, "playback to advance to the first statement");
                sendIcSoon(">");
            }
            break;
        case Phase::NextOne:
            if (l_text == "First statement") {
                if (l_char_id != "-1") {
                    fail("a replayed statement kept char id " + l_char_id);
                    return;
                }
                phase(Phase::NextTwo, "playback to advance to the second statement");
                sendIcSoon(">");
            }
            break;
        case Phase::NextTwo:
            if (l_text == "Second statement") {
                phase(Phase::LoopAround, "playback to loop back to the first statement");
                sendIcSoon(">");
            }
            break;
        case Phase::LoopAround:
            if (l_text == "First statement") {
                if (!m_saw_loop_notice) {
                    fail("playback looped without announcing it");
                    return;
                }
                phase(Phase::StayAtFirst, "playback to stay at the first statement");
                sendIcSoon("<");
            }
            break;
        case Phase::StayAtFirst:
            if (l_text == "First statement") {
                phase(Phase::JumpTwo, "playback to jump to statement two");
                sendIcSoon(">2");
            }
            break;
        case Phase::JumpTwo:
            if (l_text == "Second statement") {
                phase(Phase::UpdatePrompt, "the update prompt");
                m_leader->sendOoc("/update");
            }
            break;
        case Phase::UpdateSend:
            if (l_text == "Second statement, revised") {
                if (l_char_id != l_leader_id) {
                    fail("the live replacement lost its speaker: char id " + l_char_id);
                    return;
                }
                m_update_echoed = true;
                finishUpdateWhenBothArrived();
            }
            break;
        case Phase::AddSend:
            if (l_text == "Exhibit #1 at 100%") {
                say("the separator characters arrived intact in the room");
                phase(Phase::ListAll, "the full testimony listing");
                m_leader->sendOoc("/testimony");
            }
            break;
        case Phase::ExamineLoaded:
            if (l_text == "~~-- The Turnabout --") {
                if (l_char_id != "-1") {
                    fail("the loaded title replayed with char id " + l_char_id);
                    return;
                }
                phase(Phase::NavigateLoaded, "the loaded testimony to play back");
                sendIcSoon(">");
            }
            break;
        case Phase::NavigateLoaded:
            if (l_text == "First statement") {
                if (l_char_id != "-1") {
                    fail("a loaded statement replayed with char id " + l_char_id);
                    return;
                }
                say("record, playback, edits and the save/load round trip all line up");
                m_watchdog->stop();
                QTimer::singleShot(500, qApp, [] { qApp->exit(0); });
            }
            break;
        default:
            break;
        }
    }

    // The update confirmation reaches only the leader while the broadcast
    // reaches the witness, on two sockets with no order between them.
    void finishUpdateWhenBothArrived()
    {
        if (m_update_confirmed && m_update_echoed) {
            phase(Phase::AddPrompt, "the insert prompt");
            m_leader->sendOoc("/add");
        }
    }

    void phase(Phase f_phase, const QString &f_waiting_for)
    {
        m_phase = f_phase;
        m_waiting_for = f_waiting_for;
        say("--- waiting for " + f_waiting_for + " ---");
        m_watchdog->start(15000);
    }

    void say(const QString &f_text)
    {
        std::cout << f_text.toStdString() << std::endl;
    }

    void fail(const QString &f_reason)
    {
        say("FAILED: " + f_reason);
        qApp->exit(1);
    }

    QString m_address;
    int m_port;
    QString m_modpass;
    QString m_save_name;
    QTimer *m_watchdog;
    QString m_waiting_for;
    DanceClient *m_leader = nullptr;
    DanceClient *m_witness = nullptr;
    Phase m_phase = Phase::LeaderLogin;
    QString m_last_rt;
    bool m_saw_loop_notice = false;
    bool m_update_confirmed = false;
    bool m_update_echoed = false;
};

// The jukebox scene, played by two clients: a moderator sets up two short
// custom songs and turns the jukebox on, a witness proves what the room
// hears. The load-bearing assertions:
//   1. the first request starts playback right away, broadcast as an MC
//      with char id -1 (the jukebox, not any player, owns the music);
//   2. after the two-second song ends the jukebox moves on BY ITSELF -
//      the timer-driven pick reaches the room with no player acting;
//   3. /jukebox_skip forces the next pick immediately;
//   4. with the jukebox off, the same request is a normal music change
//      again, carrying the requesting player's own char id.
class JukeboxDance : public QObject
{
    Q_OBJECT

  public:
    JukeboxDance(const QString &f_address, int f_port, const QString &f_modpass, QObject *parent = nullptr) :
        QObject(parent),
        m_address(f_address),
        m_port(f_port),
        m_modpass(f_modpass)
    {
        m_watchdog = new QTimer(this);
        m_watchdog->setSingleShot(true);
        connect(m_watchdog, &QTimer::timeout, this, [this] { fail("timed out waiting for: " + m_waiting_for); });

        phase(Phase::LeaderLogin, "the leader to join and log in");
        m_leader = new DanceClient("leader", "Phoenix", m_address, m_port, 0, this);
        connect(m_leader, &DanceClient::failed, this, [this](const QString &f_reason) { fail(f_reason); });
        connect(m_leader, &DanceClient::ready, this, [this] { m_leader->sendOoc("/login"); });
        connect(m_leader, &DanceClient::oocReceived, this, &JukeboxDance::onLeaderOoc);
        connect(m_leader, &DanceClient::loggedIn, this, [this] {
            phase(Phase::WitnessJoin, "the witness to join");
            m_witness = new DanceClient("witness", "", m_address, m_port, 0, this);
            connect(m_witness, &DanceClient::failed, this, [this](const QString &f_reason) { fail(f_reason); });
            connect(m_witness, &DanceClient::ready, this, [this] {
                phase(Phase::ClearCustoms, "leftover custom songs to clear");
                m_leader->sendOoc("/clearcustommusic");
            });
            connect(m_witness, &DanceClient::mcReceived, this, &JukeboxDance::onWitnessMc);
        });
    }

  private:
    enum class Phase
    {
        LeaderLogin,
        WitnessJoin,
        ClearCustoms,
        AddFirstSong,
        AddSecondSong,
        EnableJukebox,
        FirstRequest,
        SecondRequest,
        AutoSwitch,
        Skip,
        DisableJukebox,
        PlainMusicChange,
    };

    void requestSong(const QString &f_song)
    {
        m_leader->send(AOPacket("MC", {f_song, QString::number(m_leader->charId()), "", "0"}));
    }

    void onLeaderOoc(const QString &f_message)
    {
        if (f_message.contains("Entering login prompt")) {
            m_leader->sendOoc(m_modpass);
            return;
        }

        switch (m_phase) {
        case Phase::ClearCustoms:
            if (f_message.contains("Custom songs have been cleared")) {
                phase(Phase::AddFirstSong, "the first custom song");
                m_leader->sendOoc("/addmusic jukeboxtest-a,jukeboxtest-a,2");
            }
            break;
        case Phase::AddFirstSong:
            if (f_message.contains("addition of the song has succeeded")) {
                phase(Phase::AddSecondSong, "the second custom song");
                m_leader->sendOoc("/addmusic jukeboxtest-b,jukeboxtest-b,2");
            }
            else if (f_message.contains("addition of the song has failed")) {
                fail("could not add the custom song");
            }
            break;
        case Phase::AddSecondSong:
            if (f_message.contains("addition of the song has succeeded")) {
                phase(Phase::EnableJukebox, "the jukebox to switch on");
                m_leader->sendOoc("/togglejukebox");
            }
            break;
        case Phase::EnableJukebox:
            if (f_message.contains("jukebox in this area has been disabled")) {
                // A previous run left it on; the toggle also cleared it.
                m_leader->sendOoc("/togglejukebox");
            }
            else if (f_message.contains("jukebox in this area has been enabled")) {
                phase(Phase::FirstRequest, "the first request to start playback");
                requestSong("jukeboxtest-a.opus");
            }
            break;
        case Phase::SecondRequest:
            if (f_message.contains("Song added to Jukebox")) {
                phase(Phase::AutoSwitch, "the jukebox to move on by itself after the song ends");
            }
            break;
        case Phase::DisableJukebox:
            if (f_message.contains("jukebox in this area has been disabled")) {
                phase(Phase::PlainMusicChange, "the request to be a normal music change again");
                requestSong("jukeboxtest-a.opus");
            }
            break;
        default:
            break;
        }
    }

    void onWitnessMc(const QStringList &f_fields)
    {
        const QString l_song = f_fields.value(0);
        if (!l_song.startsWith("jukeboxtest-")) {
            // Join-time catch-up music and other rooms' noise.
            return;
        }

        switch (m_phase) {
        case Phase::FirstRequest:
            if (l_song == "jukeboxtest-a.opus") {
                if (f_fields.value(1) != "-1") {
                    fail("the jukebox broadcast carries char id " + f_fields.value(1) + " instead of -1");
                    return;
                }
                phase(Phase::SecondRequest, "the second song to queue up");
                requestSong("jukeboxtest-b.opus");
            }
            break;
        case Phase::AutoSwitch:
            if (f_fields.value(1) == "-1") {
                say("the two-second song ended and the jukebox moved on by itself");
                phase(Phase::Skip, "the skip to force the next pick");
                m_leader->sendOoc("/jukebox_skip");
            }
            break;
        case Phase::Skip:
            if (f_fields.value(1) == "-1") {
                phase(Phase::DisableJukebox, "the jukebox to switch off");
                m_leader->sendOoc("/togglejukebox");
            }
            break;
        case Phase::PlainMusicChange:
            if (l_song == "jukeboxtest-a.opus") {
                if (f_fields.value(1) != QString::number(m_leader->charId())) {
                    fail("the plain music change lost its player: char id " + f_fields.value(1));
                    return;
                }
                say("request, playback, self-switching, skip and the plain path all line up");
                m_watchdog->stop();
                QTimer::singleShot(500, qApp, [] { qApp->exit(0); });
            }
            break;
        default:
            break;
        }
    }

    void phase(Phase f_phase, const QString &f_waiting_for)
    {
        m_phase = f_phase;
        m_waiting_for = f_waiting_for;
        say("--- waiting for " + f_waiting_for + " ---");
        m_watchdog->start(15000);
    }

    void say(const QString &f_text)
    {
        std::cout << f_text.toStdString() << std::endl;
    }

    void fail(const QString &f_reason)
    {
        say("FAILED: " + f_reason);
        qApp->exit(1);
    }

    QString m_address;
    int m_port;
    QString m_modpass;
    QTimer *m_watchdog;
    QString m_waiting_for;
    DanceClient *m_leader = nullptr;
    DanceClient *m_witness = nullptr;
    Phase m_phase = Phase::LeaderLogin;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString l_address = app.arguments().value(1, "127.0.0.1");
    const int l_port = app.arguments().value(2, "27016").toInt();
    const QString l_modpass = app.arguments().value(3, "changeme");
    const QString l_mode = app.arguments().value(4, "classic");
    if (l_mode == "evidence") {
        EvidenceDance l_dance(l_address, l_port, l_modpass);
        return app.exec();
    }
    if (l_mode == "testimony") {
        TestimonyDance l_dance(l_address, l_port, l_modpass);
        return app.exec();
    }
    if (l_mode == "jukebox") {
        JukeboxDance l_dance(l_address, l_port, l_modpass);
        return app.exec();
    }
    if (l_mode != "classic") {
        std::cout << "unknown mode: " << l_mode.toStdString() << " (use classic, evidence, testimony or jukebox)" << std::endl;
        return 1;
    }
    MiniClient l_client(l_address, l_port, l_modpass);
    return app.exec();
}

#include "main.moc"
