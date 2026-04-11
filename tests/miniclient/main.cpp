// AI-generated: written by Claude.
// A headless AO2 client built on the real client's netcode. It joins a
// running akashi server the same way the desktop client does, then walks
// the playtest checklist: pick a character, chat in OOC and IC, change
// the music, throw a judge splash, present evidence with every escape
// character in it, keepalive, get denied a mod command, fail a login, log
// in with the modpass and run a database-backed mod command. Exits 0
// when every step got the expected answer, 1 on a timeout or failure.
// Usage: ao2_miniclient [address] [port] [modpass] [classic]
#include "aopacket.h"
#include "websocketconnection.h"

#include <QCoreApplication>
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
                m_done = true;
                m_watchdog->stop();
                say("every step answered, disconnecting");
                m_connection->disconnectFromServer();
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString l_address = app.arguments().value(1, "127.0.0.1");
    const int l_port = app.arguments().value(2, "27016").toInt();
    const QString l_modpass = app.arguments().value(3, "changeme");
    MiniClient l_client(l_address, l_port, l_modpass);
    return app.exec();
}

#include "main.moc"
