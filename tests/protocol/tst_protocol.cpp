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
    void preJoinPacketsAreDropped();
    void configRuleBlocksIc();
    void pairRequestsClearOnCharacterAndAreaChanges();
    void testimonyPlaybackJumpRefreshesTheEvidenceList();
    void forcedPositionSanitizesAndRefreshesTheTarget();
    void ruleCommandsGovernIc();
    void ruleMutationRefusals();
    void timedSanctionLiftsItself();
    void plainSanctionSurvivesReconnect();
    void areaGrantOpensACommandThere();
    void lockedAreasGateEntryThroughTheEnterOffer();
    void whyExplainsResolutionAndDumpGrantsLists();
    void modsListsPresenceWithoutTheRadar();
    void everyoneSectionEditsTheOrdinaryBaseline();
    void dynamicFloorsAndAreas();
    void defaultFloorGatesEnforceAreaPolicy();
    void savedWorldSurvivesAReload();
    void floorFileRoundTrip();
    void authPacketAuthenticatesAgainstTheActiveSystem();
    void advancedBootCreatesARootAccount();
    void emptyModpassBootGeneratesOne();
    void oocRoundtrip();
    void areaMessageRejectsOutOfRangeAreaId();
    void randomCharAssignsAFreeCharacter();
    void randomCharRefusesWhenEveryCharacterIsTaken();
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
    void loginAsModeratorWith(TestClient &client, const QString &modpass);
    // Sends a pair-carrying IC message on def and answers with the pair
    // field of the sender's own MS echo: the partner's char id when the
    // mutual match held, -1 when nobody pointed back.
    QString pairEcho(TestClient &client, const QString &charName, int charId, int pairWith, const QString &text);

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
    // The tail token names the active auth system; the fixture runs simple
    // auth, which is the password system.
    QCOMPARE(client.takeNext(),
             QStringLiteral("FL#noencryption#yellowtext#prezoom#flipping#customobjections#fastloading#deskmod#"
                            "evidence#cccc_ic_support#arup#casing_alerts#modcall_reason#looping_sfx#additive#"
                            "effects#y_offset#expanded_desk_mods#auth_packet#custom_blips#auth_password#%"));
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
    QCOMPARE(client.takeNext(), QStringLiteral("LE#%"));
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
    QCOMPARE(client.takeNext(), QStringLiteral("DONE#%"));

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

void ProtocolTest::preJoinPacketsAreDropped()
{
    TestClient client;
    performHandshake(client);

    // Past HI/ID but never RD: the user tier is not held yet, so the play
    // surface answers with nothing at all - no OOC echo, no error, and no
    // modcall reaching anyone.
    client.send(QStringLiteral("CT#Tester#hello before joining#%"));
    client.send(QStringLiteral("ZZ#pre-join modcall#-1#%"));
    QVERIFY(client.waitForIdle());
    QCOMPARE(client.pendingCount(), 0);

    // The drops leave the session intact: RD still joins cleanly.
    client.send(QStringLiteral("RD#%"));
    QVERIFY(client.waitForIdle());
    bool sawDone = false;
    while (client.pendingCount() > 0)
        sawDone |= client.takeNext() == QStringLiteral("DONE#%");
    QVERIFY(sawDone);
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
    loginAsModeratorWith(client, QStringLiteral("changeme"));
}

void ProtocolTest::loginAsModeratorWith(TestClient &client, const QString &modpass)
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
    client.send(QStringLiteral("CT#Tester#%1#%").arg(modpass));
    client.waitForIdle();
    bool loggedIn = false;
    while (client.pendingCount() > 0)
        loggedIn |= client.takeNext() == QStringLiteral("AUTH#1#%");
    QVERIFY(loggedIn);
}

QString ProtocolTest::pairEcho(TestClient &client, const QString &charName, int charId, int pairWith, const QString &text)
{
    // 19 fields: the classic base set plus showname, pair request, offset
    // and immediate. Field 16 of the echo carries the resolved partner.
    client.send(QStringLiteral("MS#chat#-#%1#normal#%2#def#1#0#%3#0#0#0#0#0#0##%4#0#0#%")
                    .arg(charName, text, QString::number(charId), QString::number(pairWith)));
    client.waitForIdle();
    QString echo;
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        if (packet.startsWith(QStringLiteral("MS#")))
            echo = packet;
    }
    // Splitting keeps empty fields, so the indexes stay aligned.
    return echo.split(QLatin1Char('#')).value(17);
}

void ProtocolTest::pairRequestsClearOnCharacterAndAreaChanges()
{
    TestClient partner;
    joinServer(partner);
    TestClient mover;
    joinServer(mover, 1);

    const auto drain = [](TestClient &client) {
        client.waitForIdle();
        while (client.pendingCount() > 0)
            client.takeNext();
    };

    // The partner plays Phoenix (1) and stays put; the mover plays
    // Edgeworth (2) and does the moving.
    partner.send(QStringLiteral("CC#0#1#TESTHWID#%"));
    mover.send(QStringLiteral("CC#0#2#TESTHWID#%"));
    drain(partner);
    drain(mover);

    // The mover asks for the pair first; nobody points back yet.
    QCOMPARE(pairEcho(mover, QStringLiteral("Edgeworth"), 2, 1, QStringLiteral("care to join me")), QStringLiteral("-1"));
    drain(partner);

    // The partner points back: both requests stand on def, so they pair.
    QCOMPARE(pairEcho(partner, QStringLiteral("Phoenix"), 1, 2, QStringLiteral("gladly")), QStringLiteral("2"));
    drain(mover);

    // The mover steps out and back in. The walk clears its pair request,
    // so the stale request no longer matches silently on the return.
    mover.send(QStringLiteral("MC#Courtroom 1#2#%"));
    drain(mover);
    drain(partner);
    mover.send(QStringLiteral("MC#Basement#2#%"));
    drain(mover);
    drain(partner);
    QCOMPARE(pairEcho(partner, QStringLiteral("Phoenix"), 1, 2, QStringLiteral("still with me")), QStringLiteral("-1"));
    drain(mover);

    // A fresh request after the return resumes the mutual match.
    QCOMPARE(pairEcho(mover, QStringLiteral("Edgeworth"), 2, 1, QStringLiteral("asking again")), QStringLiteral("1"));
    drain(partner);

    // Changing character clears the request too. The mover re-commits its
    // position with a plain message, which never touches a pair request,
    // and the partner still finds nobody pointing back.
    mover.send(QStringLiteral("CC#0#0#TESTHWID#%"));
    drain(mover);
    drain(partner);
    mover.send(QStringLiteral("MS#chat#-#Franziska#normal#back on the stand#def#1#0#0#0#0#0#0#0#0#%"));
    drain(mover);
    drain(partner);
    QCOMPARE(pairEcho(partner, QStringLiteral("Phoenix"), 1, 0, QStringLiteral("who are you")), QStringLiteral("-1"));
}

void ProtocolTest::testimonyPlaybackJumpRefreshesTheEvidenceList()
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
    const auto sendIc = [&client, &drainInto](const QString &side, const QString &text) {
        client.send(QStringLiteral("MS#chat#-#Phoenix#normal#%1#%2#1#0#1#0#0#0#0#0#0#%").arg(text, side));
        drainInto();
    };

    loginAsModerator(client);

    // Record a title on def and one statement given from wit; speaking
    // from wit moves the recorder there, like any position change.
    command(QStringLiteral("/testify"));
    QVERIFY(!received.filter(QStringLiteral("Started testimony recording")).isEmpty());
    sendIc(QStringLiteral("def"), QStringLiteral("The Turnabout"));
    sendIc(QStringLiteral("wit"), QStringLiteral("The witness saw it all"));
    command(QStringLiteral("/pause"));

    // Step back to def before the examination; this ordinary position
    // change sends its own evidence refresh, drained here.
    sendIc(QStringLiteral("def"), QStringLiteral("back at the bench"));
    command(QStringLiteral("/examine"));

    // The > jump replays the statement recorded on wit: the jumper follows
    // it there, so the side-dependent evidence list refreshes before the
    // replayed message arrives - exactly like any other position change.
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#>#def#1#0#1#0#0#0#0#0#0#%"));
    drainInto();
    int evidenceAt = -1;
    int replayAt = -1;
    for (int i = 0; i < received.size(); i++) {
        if (evidenceAt == -1 && received.at(i).startsWith(QStringLiteral("LE#")))
            evidenceAt = i;
        if (received.at(i).startsWith(QStringLiteral("MS#")) && received.at(i).contains(QStringLiteral("The witness saw it all")))
            replayAt = i;
    }
    QVERIFY2(evidenceAt != -1, "the playback jump onto wit sent no evidence refresh");
    QVERIFY2(replayAt != -1, "the playback jump never replayed the statement");
    QVERIFY2(evidenceAt < replayAt, "the evidence refresh must precede the replayed statement");
}

void ProtocolTest::forcedPositionSanitizesAndRefreshesTheTarget()
{
    TestClient moderator;
    joinServer(moderator);
    TestClient target;
    joinServer(target, 1);

    const auto drain = [](TestClient &client) {
        client.waitForIdle();
        while (client.pendingCount() > 0)
            client.takeNext();
    };

    loginAsModerator(moderator);
    drain(target);

    // The moderator force-moves the second client with a traversal-prefixed
    // side. The commit site strips it, so the target lands on wit, is told
    // the stored position, and gets the evidence list for its new side.
    moderator.send(QStringLiteral("CT#Tester#/forcepos ../wit 1#%"));
    drain(moderator);
    target.waitForIdle();

    bool sawNotice = false;
    bool sawEvidence = false;
    bool sawSanitizedSp = false;
    while (target.pendingCount() > 0) {
        const QString packet = target.takeNext();
        sawNotice |= packet.contains(QStringLiteral("Position forcibly changed by CM."));
        sawEvidence |= packet.startsWith(QStringLiteral("LE#"));
        sawSanitizedSp |= packet == QStringLiteral("SP#wit#%");
    }
    QVERIFY2(sawNotice, "the force-moved target never heard about the move");
    QVERIFY2(sawEvidence, "the force-moved target got no evidence refresh for its new side");
    QVERIFY2(sawSanitizedSp, "the target's SP must carry the sanitized position");

    // Forcing the same side again with a traversal prefix commits nothing:
    // the sanitizer runs before the comparison, so the target gets the
    // notice and SP but no redundant evidence refresh.
    moderator.send(QStringLiteral("CT#Tester#/forcepos ../wit 1#%"));
    drain(moderator);
    target.waitForIdle();

    bool sawRepeatSp = false;
    bool sawRepeatEvidence = false;
    while (target.pendingCount() > 0) {
        const QString packet = target.takeNext();
        sawRepeatSp |= packet == QStringLiteral("SP#wit#%");
        sawRepeatEvidence |= packet.startsWith(QStringLiteral("LE#"));
    }
    QVERIFY2(sawRepeatSp, "the unconditional SP must still arrive on a same-side force");
    QVERIFY2(!sawRepeatEvidence, "a same-side force must not fire a redundant evidence refresh");
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
    const auto logout = [&client, &drain]() {
        client.send(QStringLiteral("CT#Tester#/logout#%"));
        drain();
    };
    const auto login = [&client, &drain]() {
        client.send(QStringLiteral("CT#Tester#/login#%"));
        drain();
        client.send(QStringLiteral("CT#Tester#changeme#%"));
        drain();
    };

    // An area rule added at runtime blocks IC on the spot - and it binds
    // the moderator who added it too: rules bind everyone, so an area
    // can be made silent, never moderator-proof.
    client.send(QStringLiteral("CT#Tester#/addrule ic_message_sent block message=The judge calls for order.#%"));
    drain();
    sendIc(QStringLiteral("order in the court"));
    QVERIFY(client.waitForIdle());
    bool sawModIc = false;
    bool sawModBlock = false;
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        sawModIc |= packet.startsWith(QStringLiteral("MS#"));
        sawModBlock |= packet.contains(QStringLiteral("The judge calls for order."));
    }
    QVERIFY(!sawModIc);
    QVERIFY(sawModBlock);

    // An ordinary player is gated the same way.
    logout();
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

    // Removing it frees the court again - no logout dance needed, since
    // the rule bound the moderator like anyone else.
    login();
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

void ProtocolTest::ruleMutationRefusals()
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

    // Reading the rules is open to every joined player...
    command(QStringLiteral("/rules"));
    QVERIFY(!received.filter(QStringLiteral("=== Rules in Basement ===")).isEmpty());

    // ... but attaching one needs modify_rules, and the refusal changed nothing.
    command(QStringLiteral("/addrule ic_message_sent block"));
    QVERIFY(!received.filter(QStringLiteral("You do not have permission to use that command.")).isEmpty());
    command(QStringLiteral("/rules"));
    QVERIFY(received.filter(QStringLiteral("ic_message_sent [before] block (area, command)")).isEmpty());

    // A moderator holds the permission, but the catalog still validates:
    // a placeless event dispatches no rule phases, and the action name
    // must exist.
    loginAsModerator(client);
    command(QStringLiteral("/addrule ban_issued block"));
    QVERIFY(!received.filter(QStringLiteral("block is a before action, but ban_issued does not dispatch before rules.")).isEmpty());
    // global_message_sent (the /g, /need, /pm, /a, /s signal) is placeless too.
    command(QStringLiteral("/addrule global_message_sent block"));
    QVERIFY(!received.filter(QStringLiteral("block is a before action, but global_message_sent does not dispatch before rules.")).isEmpty());
    command(QStringLiteral("/addrule ic_message_sent explode"));
    QVERIFY(!received.filter(QStringLiteral("There is no rule action named explode. See /ruleactions.")).isEmpty());
}

void ProtocolTest::timedSanctionLiftsItself()
{
    TestClient client;
    joinServer(client);

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

    // A two-second mute goes through the whole machinery: command parse,
    // sanction flag, database row, scheduled lift.
    client.send(QStringLiteral("CT#Tester#/mute 0 2s#%"));
    drain();
    sendIc(QStringLiteral("silenced"));
    QVERIFY(client.waitForIdle());
    QVERIFY(!icEchoed());

    // The scheduler lifts the sanction on its own.
    QTest::qWait(2600);
    drain();
    sendIc(QStringLiteral("speech returns"));
    QVERIFY(client.waitForIdle());
    QVERIFY(icEchoed());
}

void ProtocolTest::plainSanctionSurvivesReconnect()
{
    const auto drain = [](TestClient &client) {
        client.waitForIdle();
        while (client.pendingCount() > 0)
            client.takeNext();
    };
    const auto sendIc = [](TestClient &client, const QString &text) {
        client.send(QStringLiteral("MS#chat#-#Phoenix#normal#%1#def#1#0#1#0#0#0#0#0#0#%").arg(text));
    };
    const auto icEchoed = [](TestClient &client) {
        bool sawIc = false;
        while (client.pendingCount() > 0)
            sawIc |= client.takeNext().startsWith(QStringLiteral("MS#"));
        return sawIc;
    };
    const auto tookCharacter = [](TestClient &client) {
        bool sawPv = false;
        while (client.pendingCount() > 0)
            sawPv |= client.takeNext().startsWith(QStringLiteral("PV#"));
        return sawPv;
    };

    // A moderator plain-mutes, charcurses and OOC-mutes themselves - no
    // duration on any, the exact shapes that used to live in memory only.
    {
        TestClient client;
        joinServer(client);
        loginAsModerator(client);
        client.send(QStringLiteral("CT#Tester#/mute 0#%"));
        drain(client);
        client.send(QStringLiteral("CT#Tester#/charcurse 0#%"));
        drain(client);
        client.send(QStringLiteral("CT#Tester#/ooc_mute 0#%"));
        drain(client);
        sendIc(client, QStringLiteral("silenced"));
        QVERIFY(client.waitForIdle());
        QVERIFY(!icEchoed(client));
        client.close();
        QVERIFY(client.waitForDisconnect());
    }
    QTest::qWait(250);

    // The reconnect arrives as a brand-new session; the stored rows come
    // back by IPID and HWID before the client can act. The login door
    // stays open despite the restored OOC mute - a stored mute must never
    // brick authentication - and the curse list survived with its
    // character: Phoenix (the cursed character) can be taken, so the
    // login walks through...
    TestClient reborn;
    joinServer(reborn);
    loginAsModerator(reborn);
    // ... while Franziska is refused - the curse answers with silence.
    reborn.send(QStringLiteral("CC#0#0#TESTHWID#%"));
    QVERIFY(reborn.waitForIdle());
    QVERIFY(!tookCharacter(reborn));
    // The IC mute survived too.
    sendIc(reborn, QStringLiteral("still silenced"));
    QVERIFY(reborn.waitForIdle());
    QVERIFY(!icEchoed(reborn));
    // Plain OOC chat is still silenced, while the moderator's commands
    // keep working through the mute.
    reborn.send(QStringLiteral("CT#Tester#any words#%"));
    QVERIFY(reborn.waitForIdle());
    bool sawMuteNotice = false;
    while (reborn.pendingCount() > 0)
        sawMuteNotice |= reborn.takeNext().contains(QStringLiteral("You are OOC muted"));
    QVERIFY(sawMuteNotice);

    // The by-IPID lift door answers honestly for an unknown identity...
    reborn.send(QStringLiteral("CT#Tester#/lift_sanction nosuchipid muted#%"));
    QVERIFY(reborn.waitForIdle());
    bool sawNoRow = false;
    while (reborn.pendingCount() > 0)
        sawNoRow |= reborn.takeNext().contains(QStringLiteral("No stored"));
    QVERIFY(sawNoRow);

    // ... and lifting by hand frees everything across the fresh session.
    reborn.send(QStringLiteral("CT#Tester#/unmute 0#%"));
    drain(reborn);
    reborn.send(QStringLiteral("CT#Tester#/uncharcurse 0#%"));
    drain(reborn);
    reborn.send(QStringLiteral("CT#Tester#/ooc_unmute 0#%"));
    drain(reborn);
    sendIc(reborn, QStringLiteral("speech returns"));
    QVERIFY(reborn.waitForIdle());
    QVERIFY(icEchoed(reborn));
    reborn.send(QStringLiteral("CT#Tester#ooc returns#%"));
    QVERIFY(reborn.waitForIdle());
    bool sawOocEcho = false;
    while (reborn.pendingCount() > 0)
        sawOocEcho |= reborn.takeNext().contains(QStringLiteral("ooc returns"));
    QVERIFY(sawOocEcho);
    reborn.send(QStringLiteral("CC#0#0#TESTHWID#%"));
    QVERIFY(reborn.waitForIdle());
    QVERIFY(tookCharacter(reborn));
}

void ProtocolTest::areaGrantOpensACommandThere()
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

    // Without a grant, /notice is a moderator tool.
    command(QStringLiteral("/notice hello"));
    QVERIFY(!received.filter(QStringLiteral("You do not have permission to use that command.")).isEmpty());

    // Courtroom 1 offers send_notice to everyone in areas.json, so the
    // same client may use the same verb there.
    client.send(QStringLiteral("CC#0#1#TESTHWID#%"));
    client.waitForIdle();
    while (client.pendingCount() > 0)
        client.takeNext();
    client.send(QStringLiteral("MC#Courtroom 1#1#%"));
    client.waitForIdle();
    while (client.pendingCount() > 0)
        client.takeNext();

    command(QStringLiteral("/notice free real estate"));
    QVERIFY(!received.filter(QStringLiteral("BB#")).isEmpty());
    QVERIFY(!received.filter(QStringLiteral("free real estate")).isEmpty());

    // The fixture also tries to offer kick to everyone here; the guard
    // rail refuses moderation powers on everyone-offers, so the hammer
    // stays closed.
    command(QStringLiteral("/kick nosuchipid because"));
    QVERIFY(!received.filter(QStringLiteral("You do not have permission to use that command.")).isEmpty());

    // area.cm is the CM *status*, not the CM *powers*: this area grants
    // the status to everyone, but a CM command stays closed until the
    // claim makes you the owner, at which point the area_owner source
    // hands over the cm.* power bundle.
    command(QStringLiteral("/testify"));
    QVERIFY(!received.filter(QStringLiteral("You do not have permission to use that command.")).isEmpty());

    // Walk-in CM is an offer too: this area grants area.cm to everyone,
    // so the classic claim works here.
    command(QStringLiteral("/cm"));
    QVERIFY(!received.filter(QStringLiteral("is now CM in this area.")).isEmpty());

    // Owning the area now resolves cm.testify through the CM bundle.
    command(QStringLiteral("/testify"));
    QVERIFY(received.filter(QStringLiteral("You do not have permission to use that command.")).isEmpty());
    command(QStringLiteral("/pause"));
    command(QStringLiteral("/uncm"));

    // The offer is the area's, not the person's: back home the verbs
    // close again - and the protected Basement offers no CM claim at all.
    client.send(QStringLiteral("MC#Basement#1#%"));
    client.waitForIdle();
    while (client.pendingCount() > 0)
        client.takeNext();
    command(QStringLiteral("/notice hello again"));
    QVERIFY(!received.filter(QStringLiteral("You do not have permission to use that command.")).isEmpty());
    command(QStringLiteral("/cm"));
    QVERIFY(!received.filter(QStringLiteral("This area does not offer CM")).isEmpty());

    // A plain settings reload keeps the offers in step with areas.json:
    // dropping the Courtroom grants must land through /reload, not just
    // /reloadrules, so the two halves of the permission surface can never
    // go stale through different doors.
    {
        QFile l_file(m_workdir->path() + QStringLiteral("/config/areas.json"));
        QVERIFY(l_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
        l_file.write(R"({
            "floors": { "Default": {} },
            "0:Basement": { "background": "gs4" },
            "1:Courtroom 1": { "background": "gs4" }
        })");
    }
    TestClient mod;
    joinServer(mod, 1);
    loginAsModerator(mod);
    mod.send(QStringLiteral("CT#Tester#/reload#%"));
    mod.waitForIdle();

    client.send(QStringLiteral("MC#Courtroom 1#1#%"));
    client.waitForIdle();
    while (client.pendingCount() > 0)
        client.takeNext();
    command(QStringLiteral("/notice stale offer"));
    QVERIFY(!received.filter(QStringLiteral("You do not have permission to use that command.")).isEmpty());
}

void ProtocolTest::lockedAreasGateEntryThroughTheEnterOffer()
{
    // A moderator locks Courtroom 1; an ordinary player's entry resolves
    // through the enter offer: refused while locked, granted by an
    // invite, and the moderator passes on their own server-scope grant.
    TestClient mod;
    joinServer(mod);
    loginAsModerator(mod);
    mod.send(QStringLiteral("MC#Courtroom 1#1#%"));
    mod.waitForIdle();
    while (mod.pendingCount() > 0)
        mod.takeNext();
    mod.send(QStringLiteral("CT#Tester#/area_lock#%"));
    mod.waitForIdle();
    while (mod.pendingCount() > 0)
        mod.takeNext();

    TestClient player;
    joinServer(player, 1);
    player.send(QStringLiteral("CC#0#2#TESTHWID#%"));
    player.waitForIdle();
    while (player.pendingCount() > 0)
        player.takeNext();
    QStringList received;
    const auto tryMove = [&player, &received]() {
        player.send(QStringLiteral("MC#Courtroom 1#2#%"));
        received.clear();
        player.waitForIdle();
        while (player.pendingCount() > 0)
            received.append(player.takeNext());
    };

    tryMove();
    QVERIFY(!received.filter(QStringLiteral("Area Courtroom 1 is locked.")).isEmpty());
    QVERIFY(received.filter(QStringLiteral("You moved to area")).isEmpty());

    // The invite is a personal grant of area.enter on that area.
    mod.send(QStringLiteral("CT#Tester#/invite 1#%"));
    mod.waitForIdle();
    while (mod.pendingCount() > 0)
        mod.takeNext();
    tryMove();
    QVERIFY(!received.filter(QStringLiteral("You moved to area")).isEmpty());

    // The moderator passes locks anywhere - the renamed bypass_locks is
    // their server-scope area.enter grant.
    mod.send(QStringLiteral("CT#Tester#/area_unlock#%"));
    mod.waitForIdle();
    while (mod.pendingCount() > 0)
        mod.takeNext();
}

void ProtocolTest::whyExplainsResolutionAndDumpGrantsLists()
{
    TestClient client;
    joinServer(client);

    QString output;
    const auto command = [&client, &output](const QString &text) {
        client.send(QStringLiteral("CT#Tester#%1#%").arg(text));
        client.waitForIdle();
        QStringList lines;
        while (client.pendingCount() > 0)
            lines.append(client.takeNext());
        output = lines.join(QStringLiteral("\n"));
    };

    // A baseline grant explains itself with its source and owner - the
    // speech baseline comes from the everyone section now, not core.
    command(QStringLiteral("/why ic_chat"));
    QVERIFY(output.contains(QStringLiteral("[+] ic_chat - granted by a server-scope grant to everyone (baseline)")));

    // A refusal names where the act does resolve - the fixture offers
    // send_notice in Courtroom 1.
    command(QStringLiteral("/why send_notice"));
    QVERIFY(output.contains(QStringLiteral("[x] send_notice")));
    QVERIFY(output.contains(QStringLiteral("It works in: Courtroom 1")));

    // A command query explains the whole gate, AND-groups and declared
    // immunity included.
    command(QStringLiteral("/why area_kick"));
    QVERIFY(output.contains(QStringLiteral("area.cm+kick")));
    QVERIFY(output.contains(QStringLiteral("Cannot target holders of area.cm.")));

    // The walk-in claim is a declared escalation, so /why cm agrees with
    // what /cm actually answers: the claim needs area.cm, this area does
    // not offer it, and the place that does is named.
    command(QStringLiteral("/why cm"));
    QVERIFY(output.contains(QStringLiteral("Also needs area.cm when claiming an empty area.")));
    QVERIFY(output.contains(QStringLiteral("[x] area.cm")));
    QVERIFY(output.contains(QStringLiteral("It works in: Courtroom 1")));

    // The target form is staff visibility, not a public probe.
    command(QStringLiteral("/why 0 ic_chat"));
    QVERIFY(output.contains(QStringLiteral("You do not have permission to use that command.")));

    // A mask is the decisive fact and is reported as such - even for a
    // moderator whose super would otherwise grant everything.
    loginAsModerator(client);
    command(QStringLiteral("/mute 0"));
    command(QStringLiteral("/why ic_chat"));
    QVERIFY(output.contains(QStringLiteral("blocked by the \"muted\" sanction")));
    command(QStringLiteral("/unmute 0"));

    // The operator dump lists stored grants with audiences and owners.
    command(QStringLiteral("/dumpgrants"));
    QVERIFY(output.contains(QStringLiteral("user -> everyone [core]")));
    QVERIFY(output.contains(QStringLiteral("send_notice -> everyone [config]")));
}

void ProtocolTest::modsListsPresenceWithoutTheRadar()
{
    // A moderator is online; a second, ordinary client asks who is
    // around. Presence and identity are for everyone - the per-mod
    // area and character are the staff radar and stay with staff.
    TestClient mod;
    joinServer(mod);
    loginAsModerator(mod);

    TestClient player;
    joinServer(player, 1);

    const auto modsOutput = [](TestClient &client) {
        client.send(QStringLiteral("CT#Tester#/mods#%"));
        client.waitForIdle();
        QStringList lines;
        while (client.pendingCount() > 0)
            lines.append(client.takeNext());
        return lines.join(QStringLiteral("\n"));
    };

    const QString l_player_view = modsOutput(player);
    QVERIFY(l_player_view.contains(QStringLiteral("OOC name:")));
    QVERIFY(l_player_view.contains(QStringLiteral("Total online:")));
    QVERIFY(!l_player_view.contains(QStringLiteral("Area:")));
    QVERIFY(!l_player_view.contains(QStringLiteral("Character:")));

    // The moderator's own view keeps the radar.
    const QString l_mod_view = modsOutput(mod);
    QVERIFY(l_mod_view.contains(QStringLiteral("Area:")));
    QVERIFY(l_mod_view.contains(QStringLiteral("Character:")));

    // The same split governs /getarea's IPID column.
    const auto getareaOutput = [](TestClient &client) {
        client.send(QStringLiteral("CT#Tester#/getarea#%"));
        client.waitForIdle();
        QStringList lines;
        while (client.pendingCount() > 0)
            lines.append(client.takeNext());
        return lines.join(QStringLiteral("\n"));
    };
    QVERIFY(!getareaOutput(player).contains(QStringLiteral("): ")));
    QVERIFY(getareaOutput(mod).contains(QStringLiteral("): ")));
}

void ProtocolTest::everyoneSectionEditsTheOrdinaryBaseline()
{
    // The fixture's everyone section grants the ordinary operations but
    // deliberately omits roleplay.8ball: the baseline is config now, so a
    // dropped act refuses for an ordinary player while its neighbors keep
    // working, and staff pass on their own grants.
    TestClient player;
    joinServer(player);

    const auto oocOutput = [](TestClient &client, const QString &line) {
        client.send(QStringLiteral("CT#Tester#") + line + QStringLiteral("#%"));
        client.waitForIdle();
        QStringList lines;
        while (client.pendingCount() > 0)
            lines.append(client.takeNext());
        return lines.join(QStringLiteral("\n"));
    };

    // A granted operation resolves through the config-defined baseline.
    QVERIFY(oocOutput(player, QStringLiteral("/roll")).contains(QStringLiteral("rolled a")));

    // The dropped one refuses at the gate, and /why names the real
    // permission instead of a blanket user tier.
    QVERIFY(oocOutput(player, QStringLiteral("/8ball test")).contains(QStringLiteral("You do not have permission")));
    const QString l_why = oocOutput(player, QStringLiteral("/why 8ball"));
    QVERIFY(l_why.contains(QStringLiteral("roleplay.8ball")));

    // The blanket still covers the dropped act for staff.
    TestClient mod;
    joinServer(mod, 1);
    loginAsModerator(mod);
    QVERIFY(!oocOutput(mod, QStringLiteral("/8ball test")).contains(QStringLiteral("You do not have permission")));

    // The whitelist lockdown: an everyone section granting only OOC
    // speech closes even the base capabilities - and login stays open,
    // the one door config cannot shut.
    {
        QFile l_file(m_workdir->path() + QStringLiteral("/config/permissions.json"));
        QVERIFY(l_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
        l_file.write(R"({ "everyone": { "ooc_chat": "true" } })");
    }
    QVERIFY(oocOutput(mod, QStringLiteral("/reload")).contains(QStringLiteral("Reloaded configurations")));

    QVERIFY(oocOutput(player, QStringLiteral("/roll")).contains(QStringLiteral("You do not have permission")));
    QVERIFY(oocOutput(player, QStringLiteral("/getarea")).contains(QStringLiteral("You do not have permission")));

    // Whitelisting in through the login door reopens the acts.
    player.send(QStringLiteral("CT#Tester#/login#%"));
    player.waitForIdle();
    while (player.pendingCount() > 0)
        player.takeNext();
    player.send(QStringLiteral("CT#Tester#changeme#%"));
    player.waitForIdle();
    bool loggedIn = false;
    while (player.pendingCount() > 0)
        loggedIn |= player.takeNext() == QStringLiteral("AUTH#1#%");
    QVERIFY(loggedIn);
    QVERIFY(oocOutput(player, QStringLiteral("/roll")).contains(QStringLiteral("rolled a")));
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

    // Turning blankposting off arms the check_blankposting floor rule;
    // rules bind everyone now, but the logout also proves the gate holds
    // for the plainest possible client.
    loginAsModerator(client);
    client.send(QStringLiteral("CT#Tester#/allow_blankposting#%"));
    drainInto();
    client.send(QStringLiteral("CT#Tester#/logout#%"));
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

    // The saved floor rule came back as a config rule and still bites - for
    // an ordinary player, so the moderator logs out first.
    command(QStringLiteral("/rules"));
    QVERIFY(!received.filter(QStringLiteral("ic_message_sent [before] block (floor, config)")).isEmpty());
    command(QStringLiteral("/logout"));
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

    // The client landed back on the restored floor; the file's rule bites -
    // for an ordinary player, so the moderator logs out first.
    command(QStringLiteral("/rules"));
    QVERIFY(!received.filter(QStringLiteral("ic_message_sent [before] block (floor, config)")).isEmpty());
    command(QStringLiteral("/logout"));
    client.send(QStringLiteral("MS#chat#-#Phoenix#normal#quiet#def#1#0#1#0#0#0#0#0#0#%"));
    received.clear();
    client.waitForIdle();
    while (client.pendingCount() > 0)
        received.append(client.takeNext());
    QVERIFY(received.filter(QRegularExpression(QStringLiteral("^MS#"))).isEmpty());
    QVERIFY(!received.filter(QStringLiteral("The tower is sealed.")).isEmpty());
}

void ProtocolTest::authPacketAuthenticatesAgainstTheActiveSystem()
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

    // A wrong password is an honest refusal.
    client.send(QStringLiteral("AUTH#password#wrongpass#%"));
    drainInto();
    QVERIFY(received.contains(QStringLiteral("AUTH#0#%")));
    QVERIFY(!received.filter(QStringLiteral("Incorrect password.")).isEmpty());

    // Naming a system the server does not run never reaches the verb.
    client.send(QStringLiteral("AUTH#username#a#b#%"));
    drainInto();
    QVERIFY(received.contains(QStringLiteral("AUTH#0#%")));
    QVERIFY(!received.filter(QStringLiteral("This server uses password authentication.")).isEmpty());

    // The right system with the fixture modpass logs in.
    client.send(QStringLiteral("AUTH#password#changeme#%"));
    drainInto();
    QVERIFY(received.contains(QStringLiteral("AUTH#1#%")));

    // A second attempt hits the already-authenticated guard.
    client.send(QStringLiteral("AUTH#password#changeme#%"));
    drainInto();
    QVERIFY(!received.filter(QStringLiteral("You are already logged in!")).isEmpty());
    QVERIFY(!received.contains(QStringLiteral("AUTH#1#%")));
}

void ProtocolTest::advancedBootCreatesARootAccount()
{
    // The bootstrap needs auth=advanced BEFORE boot: stop the simple-auth
    // server init() started and rewrite the staged config. The first boot
    // already converted config.ini to config.json, so the converted file
    // goes away for the rewritten ini to convert again.
    // waitForStarted() only proves the process spawned; connecting proves
    // the boot (and the config migration) finished before the kill.
    {
        TestClient l_boot_barrier;
        QVERIFY2(l_boot_barrier.connectTo(m_port), "the init() server never came up");
        l_boot_barrier.close();
    }
    m_server->kill();
    QVERIFY(m_server->waitForFinished(5000));
    m_server->readAll();
    QVERIFY(QFile::remove(m_workdir->path() + "/config/config.json"));
    {
        QSettings config(m_workdir->path() + "/config/config.ini", QSettings::IniFormat);
        config.setValue("Options/auth", "advanced");
        config.sync();
        QCOMPARE(config.status(), QSettings::NoError);
    }
    m_server->start(QStringLiteral(AKASHI_BINARY_PATH), QStringList());
    QVERIFY2(m_server->waitForStarted(10000), "akashi binary failed to restart");

    // The banner prints the one-time root password on the server's merged
    // output; the password is extracted straight from it.
    QByteArray output;
    QRegularExpressionMatch match;
    static const QRegularExpression passwordLine(QStringLiteral("Password: ([0-9a-f]+)"));
    QDeadlineTimer deadline(15000);
    while (!deadline.hasExpired()) {
        m_server->waitForReadyRead(250);
        output += m_server->readAll();
        match = passwordLine.match(QString::fromUtf8(output));
        if (match.hasMatch())
            break;
    }
    QVERIFY2(match.hasMatch(), qPrintable("no root banner in the server output:\n" + QString::fromUtf8(output)));
    const QString rootPassword = match.captured(1);

    // The advanced boot advertises the username system; performHandshake
    // pins the simple-auth FL bytes, so the handshake runs by hand here.
    TestClient client;
    QVERIFY2(client.connectTo(m_port), "could not connect to the server");
    QCOMPARE(client.takeNext(), QStringLiteral("decryptor#NOENCRYPT#%"));
    client.send(QStringLiteral("HI#TESTHWID#%"));
    client.takeNext();
    client.send(QStringLiteral("ID#AO2#2.10.0#%"));
    client.takeNext();
    const QString features = client.takeNext();
    QVERIFY2(features.contains(QStringLiteral("#auth_username#")), qPrintable("FL must advertise auth_username: " + features));
    client.takeNext();
    client.send(QStringLiteral("askchaa#%"));
    client.takeNext();
    client.send(QStringLiteral("RC#%"));
    client.takeNext();
    client.send(QStringLiteral("RM#%"));
    client.takeNext();
    client.send(QStringLiteral("RD#%"));
    QVERIFY(client.waitForIdle());
    while (client.pendingCount() > 0)
        client.takeNext();

    // The extracted password logs in over the ordinary OOC prompt: the
    // bootstrap account works end to end.
    client.send(QStringLiteral("CT#Tester#/login#%"));
    client.waitForIdle();
    while (client.pendingCount() > 0)
        client.takeNext();
    client.send(QStringLiteral("CT#Tester#root %1#%").arg(rootPassword));
    // The verdict arrives after an off-thread PBKDF2 derivation, which a
    // debug build can stretch well past the usual idle window.
    bool loggedIn = false;
    QStringList seen;
    QDeadlineTimer verdict(15000);
    while (!loggedIn && !verdict.hasExpired()) {
        const QString packet = client.takeNext(1000);
        if (packet.isEmpty())
            continue;
        seen.append(packet);
        loggedIn |= packet == QStringLiteral("AUTH#1#%");
    }
    QVERIFY2(loggedIn, qPrintable("no AUTH#1 after the bootstrap login; saw: " + seen.join(" | ")));
}

void ProtocolTest::emptyModpassBootGeneratesOne()
{
    // Simple auth with no modpass generates one at boot instead of
    // leaving the server unloggable. Same reboot dance as the root
    // bootstrap: barrier, kill, rewrite, reboot, read the banner.
    {
        TestClient l_boot_barrier;
        QVERIFY2(l_boot_barrier.connectTo(m_port), "the init() server never came up");
        l_boot_barrier.close();
    }
    m_server->kill();
    QVERIFY(m_server->waitForFinished(5000));
    m_server->readAll();
    QVERIFY(QFile::remove(m_workdir->path() + "/config/config.json"));
    {
        QSettings config(m_workdir->path() + "/config/config.ini", QSettings::IniFormat);
        config.setValue("Options/modpass", QString());
        config.sync();
        QCOMPARE(config.status(), QSettings::NoError);
    }
    m_server->start(QStringLiteral(AKASHI_BINARY_PATH), QStringList());
    QVERIFY2(m_server->waitForStarted(10000), "akashi binary failed to restart");

    QByteArray output;
    QRegularExpressionMatch match;
    static const QRegularExpression modpassLine(QStringLiteral("Modpass: ([0-9a-f]+)"));
    QDeadlineTimer deadline(15000);
    while (!deadline.hasExpired()) {
        m_server->waitForReadyRead(250);
        output += m_server->readAll();
        match = modpassLine.match(QString::fromUtf8(output));
        if (match.hasMatch())
            break;
    }
    QVERIFY2(match.hasMatch(), qPrintable("no modpass banner in the server output:\n" + QString::fromUtf8(output)));
    const QString modpass = match.captured(1);

    // The generated modpass persists: the rewritten config carries it.
    {
        TestClient l_boot_barrier;
        QVERIFY2(l_boot_barrier.connectTo(m_port), "the rebooted server never came up");
        l_boot_barrier.close();
    }
    QFile persisted(m_workdir->path() + "/config/config.json");
    QVERIFY(persisted.open(QIODevice::ReadOnly));
    QVERIFY2(persisted.readAll().contains(modpass.toUtf8()), "the generated modpass never reached config.json");

    // And it works: the ordinary prompt logs in with it.
    TestClient client;
    joinServer(client);
    loginAsModeratorWith(client, modpass);
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

void ProtocolTest::areaMessageRejectsOutOfRangeAreaId()
{
    TestClient client;
    joinServer(client);

    // /a with an out-of-range area id used to dereference a null Area (the
    // owners() check ran before any range test) and take the whole server
    // down. It must now reply with a validation error and stay alive.
    client.send(QStringLiteral("CT#Tester#/a 99999 hello#%"));
    QString error;
    QVERIFY(client.waitForIdle());
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        if (packet.contains(QStringLiteral("valid AreaID")))
            error = packet;
    }
    QVERIFY2(!error.isEmpty(), "expected a validation error for the out-of-range area id");

    // The server survived: an ordinary OOC message still round-trips.
    client.send(QStringLiteral("CT#Tester#still here#%"));
    bool echoed = false;
    QVERIFY(client.waitForIdle());
    while (client.pendingCount() > 0) {
        if (client.takeNext().contains(QStringLiteral("still here")))
            echoed = true;
    }
    QVERIFY2(echoed, "no response after the bad /a - the server may have crashed");
}

void ProtocolTest::randomCharAssignsAFreeCharacter()
{
    TestClient client;
    joinServer(client);

    // /randomchar collects the free characters and picks one in a single
    // pass; the old retry loop spun forever once every slot was taken. On a
    // fresh area at least one of the three roster characters is free, so the
    // command must confirm a selection (PV) and never refuse.
    client.send(QStringLiteral("CT#Tester#/randomchar#%"));
    QVERIFY(client.waitForIdle());
    bool confirmed = false;
    while (client.pendingCount() > 0) {
        const QString packet = client.takeNext();
        if (packet.startsWith(QStringLiteral("PV#0#CID#")))
            confirmed = true;
        QVERIFY2(!packet.contains(QStringLiteral("taken")), qPrintable("randomchar refused: " + packet));
    }
    QVERIFY2(confirmed, "randomchar did not confirm a character selection");
}

void ProtocolTest::randomCharRefusesWhenEveryCharacterIsTaken()
{
    // The fixture roster has three characters (Franziska/Phoenix/Edgeworth),
    // so three clients can take the whole area between them.
    TestClient a, b, c;
    joinServer(a);
    joinServer(b, 1);
    joinServer(c, 2);

    const auto takeOne = [](TestClient &client) {
        client.send(QStringLiteral("CT#Tester#/randomchar#%"));
        bool confirmed = false;
        client.waitForIdle();
        while (client.pendingCount() > 0) {
            if (client.takeNext().startsWith(QStringLiteral("PV#")))
                confirmed = true;
        }
        QVERIFY2(confirmed, "a client could not take a free character");
    };
    takeOne(a);
    takeOne(b);
    takeOne(c);

    // Drop the cross-client player-list churn so the final window is clean.
    for (TestClient *client : {&a, &b, &c}) {
        client->waitForIdle();
        while (client->pendingCount() > 0)
            client->takeNext();
    }

    // Every character is now taken. /randomchar must refuse promptly rather
    // than spin the server's main thread forever. Client and server are
    // separate processes, so a reverted fix trips this idle deadline and
    // fails the assertion here instead of hanging the test.
    a.send(QStringLiteral("CT#Tester#/randomchar#%"));
    QString refusal;
    QVERIFY(a.waitForIdle());
    while (a.pendingCount() > 0) {
        const QString packet = a.takeNext();
        if (packet.contains(QStringLiteral("already taken")))
            refusal = packet;
    }
    QVERIFY2(!refusal.isEmpty(), "expected a refusal once every character was taken");
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
