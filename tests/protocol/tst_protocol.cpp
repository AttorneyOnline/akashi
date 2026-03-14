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

  private slots:
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

  private slots:
    void init();
    void cleanup();

    void handshakeSequence();
    void joinBurst();
    void oocRoundtrip();
    void escapeCodeRoundtrip();
    void oversizedFrameDisconnects();

  private:
    // Runs and checks the handshake up to (not including) RD.
    void performHandshake(TestClient &client, int expectedPlayers = 0);
    // Handshake plus RD, ignoring the join packets.
    void joinServer(TestClient &client, int expectedPlayers = 0);

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
    QVERIFY2(id.startsWith(QStringLiteral("ID#")) && id.endsWith(QStringLiteral("#akashi#jackfruit (1.9)#%")),
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

    // Join packets in send order.
    QCOMPARE(client.takeNext(), QStringLiteral("CharsCheck#0#0#0#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("LE##%"));
    QCOMPARE(client.takeNext(), QStringLiteral("HP#1#10#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("HP#2#10#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("FA#Basement#Courtroom 1#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("DONE##%"));
    QCOMPARE(client.takeNext(), QStringLiteral("BN#gs4##%"));

    const QString motd = client.takeNext();
    QVERIFY2(motd.startsWith(QStringLiteral("CT#")) && motd.contains(QStringLiteral("MOTD")),
             qPrintable("unexpected MOTD packet: " + motd));

    // One ARUP per type: player count, status, CM, lock.
    // The count is still 0 because fullArup runs before addClient.
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#0#0#0#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#1#IDLE#IDLE#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#2#FREE#FREE#%"));
    QCOMPARE(client.takeNext(), QStringLiteral("ARUP#3#FREE#FREE#%"));

    // Global timer + per-area user timers, all inactive on a fresh server.
    QCOMPARE(client.takeNext(), QStringLiteral("TI#0#3#%"));

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
