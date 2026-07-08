// AI-generated: written by Claude.
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

// Protocol regression tests. Starts the real server and compares network traffic against recorded output.
// Set AKASHI_RECORD_PACKETS=1 to print received packets when the recordings need updating.

#include <QProcess>
#include <QSettings>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QtTest>

namespace {

bool copyRecursively(const QString &src, const QString &dst)
{
    QDir source(src);
    if (!source.exists())
        return false;
    QDir().mkpath(dst);
    const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString target = dst + "/" + entry.fileName();
        if (entry.isDir()) {
            if (!copyRecursively(entry.absoluteFilePath(), target))
                return false;
        }
        else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            return false;
        }
    }
    return true;
}

bool recordMode()
{
    return qEnvironmentVariableIsSet("AKASHI_RECORD_PACKETS");
}

} // namespace

// Test client. Received packets are checked in arrival order.
class TestClient : public QObject
{
    Q_OBJECT

  public:
    explicit TestClient(QObject *parent = nullptr) :
        QObject(parent)
    {
        connect(&m_socket, &QWebSocket::textMessageReceived, this, &TestClient::onFrame);
    }

    bool connectTo(quint16 port, int attempts = 50)
    {
        for (int i = 0; i < attempts; i++) {
            QSignalSpy connected(&m_socket, &QWebSocket::connected);
            m_socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
            if (connected.wait(300))
                return true;
            m_socket.abort();
            QTest::qWait(200);
        }
        return false;
    }

    void send(const QString &packet)
    {
        m_socket.sendTextMessage(packet);
    }

    // Returns the next received packet, or an empty string on timeout.
    QString takeNext(int timeoutMs = 5000)
    {
        QDeadlineTimer deadline(timeoutMs);
        while (m_received.isEmpty() && !deadline.hasExpired())
            QTest::qWait(10);
        if (m_received.isEmpty())
            return QString();
        const QString packet = m_received.takeFirst();
        if (recordMode())
            qInfo().noquote() << QStringLiteral("RECV[%1] %2").arg(m_index++).arg(packet);
        return packet;
    }

    // Waits until no new packet has arrived for quietMs.
    bool waitForIdle(int quietMs = 300, int timeoutMs = 3000)
    {
        QDeadlineTimer deadline(timeoutMs);
        int lastCount = -1;
        QDeadlineTimer quiet(quietMs);
        while (!deadline.hasExpired()) {
            QTest::qWait(25);
            if (m_received.size() != lastCount) {
                lastCount = m_received.size();
                quiet.setRemainingTime(quietMs);
            }
            else if (quiet.hasExpired()) {
                return true;
            }
        }
        return false;
    }

    int pendingCount() const { return m_received.size(); }

    bool waitForDisconnect(int timeoutMs = 5000)
    {
        if (m_socket.state() == QAbstractSocket::UnconnectedState)
            return true;
        QSignalSpy spy(&m_socket, &QWebSocket::disconnected);
        return spy.wait(timeoutMs);
    }

    void close()
    {
        m_socket.close();
    }

  private Q_SLOTS:
    void onFrame(const QString &frame)
    {
        // Split on % in case a frame contains more than one packet.
        const QStringList parts = frame.split(QLatin1Char('%'), Qt::SkipEmptyParts);
        for (const QString &part : parts)
            m_received.append(part + QLatin1Char('%'));
    }

  private:
    QWebSocket m_socket;
    QStringList m_received;
    int m_index = 0;
};

class ProtocolTest : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void init();
    void cleanup();

    void handshakeSequence();
    void joinBurst();
    void configRuleBlocksIc();
    void ruleCommandsGovernIc();
    void dynamicFloorsAndAreas();
    void defaultFloorGatesEnforceAreaPolicy();
    void savedWorldSurvivesAReload();
    void floorFileRoundTrip();
    void oocRoundtrip();
    void escapeCodeRoundtrip();
    void pipelinedFrame();
    void oversizedFrameDisconnects();

  private:
    // Runs and checks the handshake up to (not including) RD.
    void performHandshake(TestClient &client, int expectedPlayers = 0);
    // Handshake plus RD, ignoring the join packets.
    void joinServer(TestClient &client, int expectedPlayers = 0);
    // Takes a character and logs in with the fixture modpass.
    void loginAsModerator(TestClient &client);

    QTemporaryDir *m_workdir = nullptr;
    QProcess *m_server = nullptr;
    quint16 m_port = 0;
};

void ProtocolTest::init()
{
    m_workdir = new QTemporaryDir();
    QVERIFY(m_workdir->isValid());
    QVERIFY(copyRecursively(QStringLiteral(AKASHI_FIXTURES_DIR) + "/config", m_workdir->path() + "/config"));

    // Pick a free port and write it into the staged config.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    m_port = probe.serverPort();
    probe.close();
    {
        QSettings config(m_workdir->path() + "/config/config.ini", QSettings::IniFormat);
        config.setValue("Options/port", m_port);
        config.sync();
        QCOMPARE(config.status(), QSettings::NoError);
    }

    m_server = new QProcess();
    m_server->setWorkingDirectory(m_workdir->path());
    m_server->setProcessChannelMode(QProcess::MergedChannels);
    m_server->start(QStringLiteral(AKASHI_BINARY_PATH), QStringList());
    QVERIFY2(m_server->waitForStarted(10000), "akashi binary failed to start");
}

void ProtocolTest::cleanup()
{
    if (m_server) {
        if (QTest::currentTestFailed())
            qWarning().noquote() << "server output:\n"
                                 << QString::fromUtf8(m_server->readAll());
        m_server->kill();
        m_server->waitForFinished(5000);
        delete m_server;
        m_server = nullptr;
    }
    delete m_workdir;
    m_workdir = nullptr;
}

void ProtocolTest::performHandshake(TestClient &client, int expectedPlayers)
{
    QVERIFY2(client.connectTo(m_port), "could not connect to the server");

    // Sent on connect; fantacrypt is always disabled.
    QCOMPARE(client.takeNext(), QStringLiteral("decryptor#NOENCRYPT#%"));

    client.send(QStringLiteral("HI#TESTHWID#%"));
    const QString id = client.takeNext();
    QVERIFY2(id.startsWith(QStringLiteral("ID#")) && id.endsWith(QStringLiteral("#akashi#kumquat (2.0)#%")),
             qPrintable("unexpected ID: " + id));

    client.send(QStringLiteral("ID#AO2#2.10.0#%"));
    QCOMPARE(client.takeNext(),
             QStringLiteral("PN#%1#100#This is a placeholder server description. Tell the world of AO who you are here!#%")
                 .arg(expectedPlayers));
    QCOMPARE(client.takeNext(),
             QStringLiteral("FL#noencryption#yellowtext#prezoom#flipping#customobjections#fastloading#deskmod#"
                            "evidence#cccc_ic_support#arup#casing_alerts#modcall_reason#looping_sfx#additive#"
                            "effects#y_offset#expanded_desk_mods#auth_packet#custom_blips#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("ASS#http://attorneyoffline.de/base/#%"));

    client.send(QStringLiteral("askchaa#%"));
    const QString si = client.takeNext();
    QVERIFY2(si.startsWith(QStringLiteral("SI#3#0#")), qPrintable("unexpected SI: " + si));

    client.send(QStringLiteral("RC#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("SC#Franziska#Phoenix#Edgeworth#%"));

    client.send(QStringLiteral("RM#%"));
    const QString sm = client.takeNext();
    QVERIFY2(sm.startsWith(QStringLiteral("SM#Basement#Courtroom 1#")), qPrintable("unexpected SM: " + sm));

    // The count in SI must match the number of entries in SM.
    const int advertised = si.section(QLatin1Char('#'), 3, 3).toInt();
    const int smFields = sm.chopped(2).count(QLatin1Char('#'));
    QCOMPARE(smFields, advertised);
}

void ProtocolTest::joinServer(TestClient &client, int expectedPlayers)
{
    performHandshake(client, expectedPlayers);
    client.send(QStringLiteral("RD#%"));
    QVERIFY2(client.waitForIdle(), "server did not go quiet after RD");
    while (client.pendingCount() > 0)
        client.takeNext();
}

void ProtocolTest::handshakeSequence()
{
    TestClient client;
    performHandshake(client);
}

void ProtocolTest::joinBurst()
{
    TestClient client;
    performHandshake(client);

    client.send(QStringLiteral("RD#%"));

    // Join packets in send order: the player_joined after-rules deliver the
    // area state first, then the handler closes the loading screen.
    QCOMPARE(client.takeNext(), QStringLiteral("CharsCheck#0#0#0#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("LE##%"));
    QCOMPARE(client.takeNext(), QStringLiteral("HP#1#10#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("HP#2#10#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("BN#gs4##%"));

    // The area's user timers, all inactive on a fresh server.
    QCOMPARE(client.takeNext(), QStringLiteral("TI#1#3#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("TI#2#3#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("TI#3#3#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("TI#4#3#%"));

    // The music list, the ambience, and the current song.
    QVERIFY(client.takeNext().startsWith(QStringLiteral("FM#")));
    QVERIFY(client.takeNext().startsWith(QStringLiteral("MC#")));
    QVERIFY(client.takeNext().startsWith(QStringLiteral("MC#")));

    QCOMPARE(client.takeNext(), QStringLiteral("FA#Basement#Courtroom 1#%"));

    // One ARUP per type: player count, status, CM, lock.
    // The count is still 0 because the join rules run before addClient.
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#0#0#0#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#1#IDLE#IDLE#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#2#FREE#FREE#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#3#FREE#FREE#%"));

    // The stopped global timer, then the end of the loading screen.
    QCOMPARE(client.takeNext(), QStringLiteral("TI#0#3#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("DONE##%"));

    const QString motd = client.takeNext();
    QVERIFY2(motd.startsWith(QStringLiteral("CT#")) && motd.contains(QStringLiteral("MOTD")),
             qPrintable("unexpected MOTD packet: " + motd));

    // The floor's server_joined config rule fires once per session.
    QVERIFY(client.takeNext().endsWith(QStringLiteral("#Welcome to the test server.#1#%")));

    // The remaining packets have no fixed order, so only their types are checked.
    QVERIFY(client.waitForIdle());
    bool sawJoinArup = false;
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        if (packet == QStringLiteral("ARUP#0#1#0#%"))
            sawJoinArup = true;
        QVERIFY2(packet.startsWith(QStringLiteral("TI#")) || packet.startsWith(QStringLiteral("PR#")) ||
                     packet.startsWith(QStringLiteral("PU#")) || packet.startsWith(QStringLiteral("ARUP#")) ||
                     packet.startsWith(QStringLiteral("FM#")) || packet.startsWith(QStringLiteral("MC#")),
                 qPrintable("unexpected packet in join tail: " + packet));
    }
    // The updated player count must arrive after addClient.
    QVERIFY(sawJoinArup);
}

void ProtocolTest::configRuleBlocksIc()
{
    TestClient client;
    joinServer(client);

    // Take a character; spectators cannot chat IC.
    client.send(QStringLiteral("CC#0#1#TESTHWID#%"));
    QVERIFY(client.waitForIdle());
    while (client.pendingCount() > 0)
        client.takeNext();

    // IC flows freely in the Basement.
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#free speech#def#1#0#1#0#0#0#0#0#0#%"));
    QVERIFY(client.waitForIdle());
    bool sawIc = false;
    while (client.pendingCount() > 0)
        sawIc |= client.takeNext().startsWith(QStringLiteral("MS#"));
    QVERIFY(sawIc);

    // Courtroom 1 declares a blocking ic_message_sent rule in areas.json.
    client.send(QStringLiteral("MC#Courtroom 1#1#%"));
    QVERIFY(client.waitForIdle());
    while (client.pendingCount() > 0)
        client.takeNext();

    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#silenced#def#1#0#1#0#0#0#0#0#0#%"));
    QVERIFY(client.waitForIdle());
    sawIc = false;
    bool sawBlock = false;
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        sawIc |= packet.startsWith(QStringLiteral("MS#"));
        sawBlock |= packet.contains(QStringLiteral("The gallery is silent."));
    }
    QVERIFY(!sawIc);
    QVERIFY(sawBlock);
}

void ProtocolTest::loginAsModerator(TestClient &client)
{
    client.send(QStringLiteral("CC#0#1#TESTHWID#%"));
    client.waitForIdle();
    while (client.pendingCount() > 0)
        client.takeNext();

    // Simple auth grants a logged-in moderator every permission.
    client.send(QStringLiteral("CT#Tester#/login#%"));
    client.waitForIdle();
    while (client.pendingCount() > 0)
        client.takeNext();
    client.send(QStringLiteral("CT#Tester#changeme#%"));
    client.waitForIdle();
    bool loggedIn = false;
    while (client.pendingCount() > 0)
        loggedIn |= client.takeNext() == QStringLiteral("AUTH#1#%");
    QVERIFY(loggedIn);
}

void ProtocolTest::ruleCommandsGovernIc()
{
    TestClient client;
    joinServer(client);

    // Helpers for the command dance below.
    const auto drain = [&client]() {
        client.waitForIdle();
        while (client.pendingCount() > 0)
            client.takeNext();
    };
    const auto sendIc = [&client](const QString &text) {
        client.send(QStringLiteral("MS#chat#-#Phoenix#normal#%1#def#1#0#1#0#0#0#0#0#0#%").arg(text));
    };
    const auto icEchoed = [&client]() {
        bool sawIc = false;
        while (client.pendingCount() > 0)
            sawIc |= client.takeNext().startsWith(QStringLiteral("MS#"));
        return sawIc;
    };

    loginAsModerator(client);

    // An area rule added at runtime blocks IC on the spot.
    client.send(QStringLiteral("CT#Tester#/addrule ic_message_sent block message=The judge calls for order.#%"));
    drain();
    sendIc(QStringLiteral("held in contempt"));
    QVERIFY(client.waitForIdle());
    bool sawIc = false;
    bool sawBlock = false;
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        sawIc |= packet.startsWith(QStringLiteral("MS#"));
        sawBlock |= packet.contains(QStringLiteral("The judge calls for order."));
    }
    QVERIFY(!sawIc);
    QVERIFY(sawBlock);

    // The listing shows the new rule beside the floor defaults.
    client.send(QStringLiteral("CT#Tester#/rules#%"));
    QVERIFY(client.waitForIdle());
    bool listed = false;
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        listed |= packet.contains(QStringLiteral("ic_message_sent [before] block (area, command)"));
    }
    QVERIFY(listed);

    // Removing it frees the court again.
    client.send(QStringLiteral("CT#Tester#/removerule ic_message_sent block#%"));
    drain();
    sendIc(QStringLiteral("objection sustained"));
    QVERIFY(client.waitForIdle());
    QVERIFY(icEchoed());

    // The same round trip works floor-wide.
    client.send(QStringLiteral("CT#Tester#/floorrule add ic_message_sent block message=The floor is sealed.#%"));
    drain();
    sendIc(QStringLiteral("sealed away"));
    QVERIFY(client.waitForIdle());
    QVERIFY(!icEchoed());
    client.send(QStringLiteral("CT#Tester#/floorrule remove ic_message_sent block#%"));
    drain();
    sendIc(QStringLiteral("free again"));
    QVERIFY(client.waitForIdle());
    QVERIFY(icEchoed());
}

void ProtocolTest::dynamicFloorsAndAreas()
{
    TestClient client;
    joinServer(client);

    const auto drainInto = [&client](QStringList &sink) {
        client.waitForIdle();
        while (client.pendingCount() > 0)
            sink.append(client.takeNext());
    };

    loginAsModerator(client);

    // A new area appears on this floor and the fresh area list arrives.
    client.send(QStringLiteral("CT#Tester#/createarea Annex#%"));
    QStringList received;
    drainInto(received);
    QVERIFY(received.contains(QStringLiteral("FA#Basement#Courtroom 1#Annex#%")));

    // A new floor comes with a starter area; walking in gets the full state.
    client.send(QStringLiteral("CT#Tester#/createfloor Tower#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("Created floor Tower")).isEmpty());

    client.send(QStringLiteral("CT#Tester#/floor Tower#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("You moved to area Unnamed Area")).isEmpty());
    // The dynamic floor carries the core join rules: state still arrives.
    QVERIFY(received.contains(QStringLiteral("FA#Unnamed Area#%")));
    QVERIFY(received.contains(QStringLiteral("BN#gs4##%")));

    // Renames show up in the listings and on the wire.
    client.send(QStringLiteral("CT#Tester#/renamearea Bell Chamber#%"));
    received.clear();
    drainInto(received);
    QVERIFY(received.contains(QStringLiteral("FA#Bell Chamber#%")));

    client.send(QStringLiteral("CT#Tester#/renamefloor Spire#%"));
    client.send(QStringLiteral("CT#Tester#/floors#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("Spire: Bell Chamber")).isEmpty());

    // The new area is fully playable.
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#it rings#def#1#0#1#0#0#0#0#0#0#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());

    // Removal refuses while somebody is standing there.
    client.send(QStringLiteral("CT#Tester#/removefloor Spire#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("Not every area on that floor is empty.")).isEmpty());
    client.send(QStringLiteral("CT#Tester#/removearea 3#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("That area is not empty.")).isEmpty());

    // A floor's last area cannot be removed on its own.
    client.send(QStringLiteral("CT#Tester#/floor Default#%"));
    received.clear();
    drainInto(received);
    client.send(QStringLiteral("CT#Tester#/removearea Bell Chamber#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("A floor needs at least one area; remove the floor instead.")).isEmpty());

    // Empty floor and empty area go; later IDs shift down cleanly.
    client.send(QStringLiteral("CT#Tester#/removefloor Spire#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("Removed floor Spire and its areas.")).isEmpty());
    client.send(QStringLiteral("CT#Tester#/removearea Annex#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("Removed area Annex.")).isEmpty());
    QVERIFY(received.contains(QStringLiteral("FA#Basement#Courtroom 1#%")));

    client.send(QStringLiteral("CT#Tester#/floors#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QStringLiteral("Default: Basement, Courtroom 1")).isEmpty());
    QVERIFY(received.filter(QStringLiteral("Spire")).isEmpty());

    // The compacted world is still fully playable.
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#order restored#def#1#0#1#0#0#0#0#0#0#%"));
    received.clear();
    drainInto(received);
    QVERIFY(!received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());
}

void ProtocolTest::defaultFloorGatesEnforceAreaPolicy()
{
    TestClient client;
    joinServer(client);

    QStringList received;
    const auto drainInto = [&client, &received]() {
        received.clear();
        client.waitForIdle();
        while (client.pendingCount() > 0)
            received.append(client.takeNext());
    };

    client.send(QStringLiteral("CC#0#1#TESTHWID#%"));
    drainInto();

    // The Basement's evidence is CM-only; a plain player is refused by the
    // check_evidence_access floor rule.
    client.send(QStringLiteral("PE#Knife#A bloody knife.#knife.png#%"));
    drainInto();
    QVERIFY(!received.filter(QStringLiteral("You are not allowed to modify the evidence here.")).isEmpty());
    QVERIFY(received.filter(QRegularExpression(QStringLiteral("^LE#K"))).isEmpty());

    // Iniswapping is off in the Basement: unknown folders are refused ...
    client.send(QStringLiteral("MS#chat#-#Nobody#normal#sneaky#def#1#0#1#0#0#0#0#0#0#%"));
    drainInto();
    QVERIFY(!received.filter(QStringLiteral("Iniswapping is not allowed in this area.")).isEmpty());
    QVERIFY(received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());

    // ... while swapping to a listed character stays allowed, as it always was.
    client.send(QStringLiteral("MS#chat#-#Edgeworth#normal#credible#def#1#0#1#0#0#0#0#0#0#%"));
    drainInto();
    QVERIFY(!received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());

    // Turning blankposting off arms the check_blankposting floor rule.
    loginAsModerator(client);
    client.send(QStringLiteral("CT#Tester#/allow_blankposting#%"));
    drainInto();
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal##def#1#0#1#0#0#0#0#0#0#%"));
    drainInto();
    QVERIFY(!received.filter(QStringLiteral("Blankposting has been forbidden in this area.")).isEmpty());
    QVERIFY(received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());
}

void ProtocolTest::savedWorldSurvivesAReload()
{
    TestClient client;
    joinServer(client);

    QStringList received;
    const auto drainInto = [&client, &received]() {
        received.clear();
        client.waitForIdle();
        while (client.pendingCount() > 0)
            received.append(client.takeNext());
    };
    const auto command = [&client, &drainInto](const QString &text) {
        client.send(QStringLiteral("CT#Tester#%1#%").arg(text));
        drainInto();
    };

    loginAsModerator(client);

    // Build up a world worth keeping: a new floor, a rule, a rename.
    command(QStringLiteral("/createfloor Tower"));
    command(QStringLiteral("/floorrule add ic_message_sent block message=The floor is sealed."));
    command(QStringLiteral("/renamearea Lounge"));
    QVERIFY(!received.filter(QStringLiteral("This area is now called Lounge.")).isEmpty());

    command(QStringLiteral("/saveareas"));
    QVERIFY(!received.filter(QStringLiteral("Saved the floors, areas and their rules to areas.json.")).isEmpty());

    // The reload rebuilds from the file; the client keeps its place by name.
    command(QStringLiteral("/loadareas"));
    QVERIFY(!received.filter(QStringLiteral("reloaded from areas.json")).isEmpty());
    QVERIFY(!received.filter(QRegularExpression(QStringLiteral("^LE#"))).isEmpty());

    command(QStringLiteral("/floors"));
    QVERIFY(!received.filter(QStringLiteral("Tower: Unnamed Area")).isEmpty());
    QVERIFY(!received.filter(QStringLiteral("Lounge")).isEmpty());

    // The saved floor rule came back as a config rule and still bites.
    command(QStringLiteral("/rules"));
    QVERIFY(!received.filter(QStringLiteral("ic_message_sent [before] block (floor, config)")).isEmpty());
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#silence#def#1#0#1#0#0#0#0#0#0#%"));
    drainInto();
    QVERIFY(received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());
    QVERIFY(!received.filter(QStringLiteral("The floor is sealed.")).isEmpty());
}

void ProtocolTest::floorFileRoundTrip()
{
    TestClient client;
    joinServer(client);

    QStringList received;
    const auto command = [&client, &received](const QString &text) {
        client.send(QStringLiteral("CT#Tester#%1#%").arg(text));
        received.clear();
        client.waitForIdle();
        while (client.pendingCount() > 0)
            received.append(client.takeNext());
    };

    loginAsModerator(client);

    // A floor worth keeping: renamed area, a floor rule, saved to its file.
    command(QStringLiteral("/createfloor Tower"));
    command(QStringLiteral("/floor Tower"));
    command(QStringLiteral("/renamearea Belfry"));
    command(QStringLiteral("/floorrule add ic_message_sent block message=The tower is sealed."));
    command(QStringLiteral("/savefloor"));
    QVERIFY(!received.filter(QStringLiteral("Saved floor Tower")).isEmpty());

    // Wreck the floor, then restore it from the file.
    command(QStringLiteral("/renamearea Attic"));
    command(QStringLiteral("/floorrule remove ic_message_sent block"));
    command(QStringLiteral("/loadfloor Tower"));
    QVERIFY(!received.filter(QStringLiteral("Loaded floor Tower from its file.")).isEmpty());

    command(QStringLiteral("/floors"));
    QVERIFY(!received.filter(QStringLiteral("Tower: Belfry")).isEmpty());

    // The client landed back on the restored floor; the file's rule bites.
    command(QStringLiteral("/rules"));
    QVERIFY(!received.filter(QStringLiteral("ic_message_sent [before] block (floor, config)")).isEmpty());
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#quiet#def#1#0#1#0#0#0#0#0#0#%"));
    received.clear();
    client.waitForIdle();
    while (client.pendingCount() > 0)
        received.append(client.takeNext());
    QVERIFY(received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());
    QVERIFY(!received.filter(QStringLiteral("The tower is sealed.")).isEmpty());
}

void ProtocolTest::oocRoundtrip()
{
    TestClient client;
    joinServer(client);

    client.send(QStringLiteral("CT#Tester#hello akashi#%"));
    // setName() sends a player list update before the OOC message is broadcast.
    QCOMPARE(client.takeNext(), QStringLiteral("PU#0#0#Tester#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("CT#Tester#hello akashi#0#%"));
}

void ProtocolTest::escapeCodeRoundtrip()
{
    TestClient client;
    joinServer(client);

    // Escape codes must survive the round trip unchanged.
    const QString escaped = QStringLiteral("a<num>b<percent>c<dollar>d<and>e");
    client.send(QStringLiteral("CT#Tester#") + escaped + QStringLiteral("#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("PU#0#0#Tester#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("CT#Tester#") + escaped + QStringLiteral("#0#%"));
}

void ProtocolTest::pipelinedFrame()
{
    TestClient client;
    joinServer(client);

    // Pick a character so the music change is allowed.
    client.send(QStringLiteral("CC#0#1#TESTHWID#%"));
    QVERIFY(client.waitForIdle());
    while (client.pendingCount() > 0)
        client.takeNext();

    // Two packets in one frame. The old framing dropped everything after
    // a leading MC packet.
    client.send(QStringLiteral("MC#Ace Attorney/Prelude/[AA] Prelude.opus#1#%CT#Tester#after the music#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("MC#Ace Attorney/Prelude/[AA] Prelude.opus#1##1#0#0#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("PU#0#0#Tester#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("CT#Tester#after the music#0#%"));
}

void ProtocolTest::oversizedFrameDisconnects()
{
    TestClient witness;
    joinServer(witness);

    TestClient client;
    joinServer(client, 1);

    // Drop the join notifications the witness received.
    QVERIFY(witness.waitForIdle());
    while (witness.pendingCount() > 0)
        witness.takeNext();

    // An oversized frame must close the connection without processing its packets.
    const QString frame = QStringLiteral("CT#Tester#hello#%") + QString(31000, QLatin1Char('A'));
    client.send(frame);
    QVERIFY(client.waitForDisconnect());

    // The witness only sees the disconnect updates, never the chat message.
    QVERIFY(witness.waitForIdle());
    while (witness.pendingCount() > 0) {
        const QString packet = witness.takeNext();
        QVERIFY2(!packet.startsWith(QStringLiteral("CT#")), qPrintable("oversized frame was processed: " + packet));
    }
}

QTEST_GUILESS_MAIN(ProtocolTest)
#include "tst_protocol.moc"
