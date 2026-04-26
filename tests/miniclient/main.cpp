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
// Usage: ao2_miniclient [address] [port] [modpass] [classic]
#include "aopacket.h"
#include "websocketconnection.h"

#include <QCoreApplication>
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
    if (l_mode != "classic") {
        std::cout << "unknown mode: " << l_mode.toStdString() << " (use classic or evidence)" << std::endl;
        return 1;
    }
    MiniClient l_client(l_address, l_port, l_modpass);
    return app.exec();
}

#include "main.moc"
