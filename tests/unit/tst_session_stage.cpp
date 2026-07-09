// AI-generated: written by Claude.
#include "core/client_session.h"
#include "fake_transport.h"
#include "proto/client_profile.h"

#include <QTest>

using SessionStage = akashi::ClientSession::SessionStage;

class tst_SessionStage : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void startsConnected();
    void advancesForward();
    void neverMovesBackward();
    void equalStageIsANoOp();
    void skipsStraightToJoined();
    void identifyAdvancesToIdentified();
    void markJoinedAdvancesToJoined();
    void doubleMarkJoinedIsANoOp();
    void identifyAfterJoinedKeepsTheStage();
    void serverlessJoinedSessionLeavesSafely();
};

void tst_SessionStage::startsConnected()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);

    QCOMPARE(l_session.stage(), SessionStage::Connected);
    QVERIFY(!l_session.isIdentified());
    QVERIFY(!l_session.isJoined());
}

void tst_SessionStage::advancesForward()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);

    l_session.advanceStage(SessionStage::Identified);
    QCOMPARE(l_session.stage(), SessionStage::Identified);
    QVERIFY(l_session.isIdentified());
    QVERIFY(!l_session.isJoined());

    l_session.advanceStage(SessionStage::Joined);
    QCOMPARE(l_session.stage(), SessionStage::Joined);
    QVERIFY(l_session.isIdentified());
    QVERIFY(l_session.isJoined());
}

void tst_SessionStage::neverMovesBackward()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);
    l_session.advanceStage(SessionStage::Joined);

    l_session.advanceStage(SessionStage::Identified);
    QCOMPARE(l_session.stage(), SessionStage::Joined);

    l_session.advanceStage(SessionStage::Connected);
    QCOMPARE(l_session.stage(), SessionStage::Joined);
    QVERIFY(l_session.isIdentified());
    QVERIFY(l_session.isJoined());
}

void tst_SessionStage::equalStageIsANoOp()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);
    l_session.advanceStage(SessionStage::Identified);

    l_session.advanceStage(SessionStage::Identified);
    QCOMPARE(l_session.stage(), SessionStage::Identified);
}

void tst_SessionStage::skipsStraightToJoined()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);

    l_session.advanceStage(SessionStage::Joined);
    QCOMPARE(l_session.stage(), SessionStage::Joined);
    QVERIFY(l_session.isIdentified());
    QVERIFY(l_session.isJoined());
}

void tst_SessionStage::identifyAdvancesToIdentified()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);

    l_session.identify(akashi::ClientProfile{});
    QCOMPARE(l_session.stage(), SessionStage::Identified);
    QVERIFY(l_session.isIdentified());
    QVERIFY(!l_session.isJoined());
}

void tst_SessionStage::markJoinedAdvancesToJoined()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);
    l_session.identify(akashi::ClientProfile{});

    l_session.markJoined();
    QCOMPARE(l_session.stage(), SessionStage::Joined);
    QVERIFY(l_session.isJoined());
}

void tst_SessionStage::doubleMarkJoinedIsANoOp()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);
    l_session.markJoined();

    l_session.markJoined();
    QCOMPARE(l_session.stage(), SessionStage::Joined);
    QVERIFY(l_session.isJoined());
}

void tst_SessionStage::identifyAfterJoinedKeepsTheStage()
{
    akashi::ClientSession l_session(nullptr, new FakeTransport(true), 1);
    l_session.identify(akashi::ClientProfile{});
    l_session.markJoined();

    // A second ID packet after the join must not drag the session back
    // to Identified; only the profile is refreshed.
    akashi::ClientProfile l_late;
    l_late.arch = "webAO";
    l_session.identify(l_late);

    QCOMPARE(l_session.stage(), SessionStage::Joined);
    QVERIFY(l_session.isJoined());
    QCOMPARE(l_session.profile().arch, QString("webAO"));
}

void tst_SessionStage::serverlessJoinedSessionLeavesSafely()
{
    // A Joined session without a server has no world to withdraw from;
    // leave() must notice the null server instead of dereferencing it,
    // both when called directly and again from the destructor.
    auto *l_session = new akashi::ClientSession(nullptr, new FakeTransport(true), 1);
    l_session->advanceStage(SessionStage::Joined);

    l_session->leave();
    delete l_session;
}

QTEST_MAIN(tst_SessionStage)
#include "tst_session_stage.moc"
