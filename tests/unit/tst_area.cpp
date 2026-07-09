// AI-generated: written by Claude.
#include "akashi/config_store.h"
#include "world/area.h"

#include <QSettings>
#include <QtTest>

Q_DECLARE_METATYPE(akashi::Area::Side);

namespace tests {
namespace unittests {

class Area : public QObject
{
    Q_OBJECT

  public:
    akashi::Area *m_area;
    akashi::ConfigStore *m_store = nullptr;
    QSettings *m_areas_ini = nullptr;

  private Q_SLOTS:
    void initTestCase();
    void init();

    void cleanup();

    void clientJoinLeave();

    void areaStatuses_data();

    void areaStatuses();

    void canonicalStatuses_data();

    void canonicalStatuses();

    void changeHP_data();

    void changeHP();

    void changeCharacter();

    void hiddenCmTagsEvidenceDescriptions();

    void takeCharacterRefusesADuplicateClaim();

    void releaseCharacterOfANeverTakenIdIsHarmless();

    void changeCharacterRefusesATakenTarget();

    void removeOwnerOfANonOwnerChangesNoOwnership();

    void removeOwnerReportsWhetherTheAreaUnlockedItself();

    void evidenceEditsIgnoreIndexesOutOfBounds();

    void notecardsStoreClearAndHandOverOnce();
};

void Area::initTestCase()
{
    m_store = new akashi::ConfigStore("config", this);
    m_areas_ini = m_store->settings("areas");
}

void Area::init()
{
    m_area = new akashi::Area("Test Area", 0, 0, 0, m_areas_ini);
}

void Area::cleanup()
{
    delete m_area;
}

void Area::clientJoinLeave()
{
    {
        // There must be exactly one client in the area, and it must have a charid of 5 and userid 0.
        m_area->addClient(5, 0);

        QCOMPARE(m_area->players().size(), 1);
        QCOMPARE(m_area->charactersTaken().at(0), 5);
    }
    {
        // No clients must be left in the area.
        m_area->removeClient(5, 0);

        QCOMPARE(m_area->players().size(), 0);
    }
}

void Area::areaStatuses_data()
{
    QTest::addColumn<QString>("statusCall");
    QTest::addColumn<QString>("expectedStatus");
    QTest::addColumn<bool>("isSuccessful");

    QTest::newRow("Idle") << "idle"
                          << "IDLE" << true;
    QTest::newRow("RP") << "rp"
                        << "RP" << true;
    QTest::newRow("Casing") << "casing"
                            << "CASING" << true;
    QTest::newRow("Looking for players (long)") << "looking-for-players"
                                                << "LOOKING-FOR-PLAYERS" << true;
    QTest::newRow("Looking for players (short)") << "lfp"
                                                 << "LOOKING-FOR-PLAYERS" << true;
    QTest::newRow("Gaming") << "gaming"
                            << "GAMING" << true;
    QTest::newRow("Recess") << "recess"
                            << "RECESS" << true;
    QTest::newRow("Nonsense") << "blah"
                              << "IDLE" << false;
}

void Area::areaStatuses()
{
    QFETCH(QString, statusCall);
    QFETCH(QString, expectedStatus);
    QFETCH(bool, isSuccessful);

    bool l_success = m_area->changeStatus(statusCall);

    QCOMPARE(m_area->status(), expectedStatus);
    QCOMPARE(l_success, isSuccessful);
}

void Area::canonicalStatuses_data()
{
    // The validation query behind changeStatus: the stored spelling for a
    // known name, nothing for an unknown one.
    QTest::addColumn<QString>("statusCall");
    QTest::addColumn<bool>("isKnown");
    QTest::addColumn<QString>("expectedSpelling");

    QTest::newRow("Idle") << "idle" << true
                          << "IDLE";
    QTest::newRow("Looking for players (short)") << "lfp" << true
                                                 << "LOOKING-FOR-PLAYERS";
    QTest::newRow("Looking for players (long)") << "looking-for-players" << true
                                                << "LOOKING-FOR-PLAYERS";
    QTest::newRow("Gaming") << "gaming" << true
                            << "GAMING";
    QTest::newRow("Nonsense") << "blah" << false << QString();
    QTest::newRow("Wrong case") << "IDLE" << false << QString();
}

void Area::canonicalStatuses()
{
    QFETCH(QString, statusCall);
    QFETCH(bool, isKnown);
    QFETCH(QString, expectedSpelling);

    const std::optional<QString> l_canonical = akashi::Area::canonicalStatus(statusCall);

    QCOMPARE(l_canonical.has_value(), isKnown);
    if (isKnown) {
        QCOMPARE(*l_canonical, expectedSpelling);
    }
    // The query alone never mutates.
    QCOMPARE(m_area->status(), QString("IDLE"));
}

void Area::changeHP_data()
{
    QTest::addColumn<akashi::Area::Side>("side");
    QTest::addColumn<int>("setHP");
    QTest::addColumn<int>("expectedHP");

    QTest::newRow("Set = Expected (DEF)") << akashi::Area::Side::DEFENCE << 3 << 3;
    QTest::newRow("Set = Expected (PRO)") << akashi::Area::Side::PROSECUTOR << 5 << 5;
    QTest::newRow("Below Zero (DEF)") << akashi::Area::Side::DEFENCE << -5 << 0;
    QTest::newRow("Below Zero (PRO)") << akashi::Area::Side::PROSECUTOR << -7 << 0;
    QTest::newRow("Above Ten (DEF)") << akashi::Area::Side::DEFENCE << 12 << 10;
    QTest::newRow("Above Ten (PRO)") << akashi::Area::Side::PROSECUTOR << 14 << 10;
}

void Area::changeHP()
{
    QFETCH(akashi::Area::Side, side);
    QFETCH(int, setHP);
    QFETCH(int, expectedHP);

    m_area->changeHP(side, setHP);

    if (akashi::Area::Side::DEFENCE == side) {
        QCOMPARE(expectedHP, m_area->defHP());
    }
    else {
        QCOMPARE(expectedHP, m_area->proHP());
    }
}

void Area::changeCharacter()
{
    {
        // A client with a charid of 6 and userid 0 joins. There's only them in there.
        m_area->addClient(6, 0);

        QCOMPARE(m_area->charactersTaken().size(), 1);
        QCOMPARE(m_area->charactersTaken().at(0), 6);
    }
    {
        // Charid 7 is marked as taken. No other client in the area still.
        // Charids 6 and 7 are taken.
        m_area->changeCharacter(-1, 7);

        QCOMPARE(m_area->playerCount(), 1);
        QCOMPARE(m_area->charactersTaken().size(), 2);
        QCOMPARE(m_area->charactersTaken().at(0), 6);
        QCOMPARE(m_area->charactersTaken().at(1), 7);
    }
    {
        // Client switches to charid 8.
        // Charids 8 and 7 are taken.
        m_area->changeCharacter(6, 8);

        QCOMPARE(m_area->playerCount(), 1);
        QCOMPARE(m_area->charactersTaken().size(), 2);
        QCOMPARE(m_area->charactersTaken().at(0), 7);
        QCOMPARE(m_area->charactersTaken().at(1), 8);
    }
    {
        // Client attempts to switch to 7.
        // Nothing changes, as it is already taken.

        m_area->changeCharacter(8, 7);

        QCOMPARE(m_area->playerCount(), 1);
        QCOMPARE(m_area->charactersTaken().size(), 2);
        QCOMPARE(m_area->charactersTaken().at(0), 7);
        QCOMPARE(m_area->charactersTaken().at(1), 8);
    }
    {
        // Charid 7 is unlocked for use.
        // Charid 8 is taken.
        m_area->changeCharacter(7, -1);

        QCOMPARE(m_area->playerCount(), 1);
        QCOMPARE(m_area->charactersTaken().size(), 1);
        QCOMPARE(m_area->charactersTaken().at(0), 8);
    }
}

void Area::hiddenCmTagsEvidenceDescriptions()
{
    // Outside hidden mode descriptions pass unchanged.
    QCOMPARE(m_area->taggedEvidenceDescription("Sharp."), QString("Sharp."));

    // Hidden mode writes the <owner=all> tag into untagged descriptions,
    // so evidence added without a tag stays visible to everyone.
    m_area->setEvidenceAccess(akashi::EvidenceStore::Access::HiddenCm);
    QCOMPARE(m_area->taggedEvidenceDescription("Sharp."), QString("<owner=all>\nSharp."));

    // An explicit owner tag is kept as sent.
    QCOMPARE(m_area->taggedEvidenceDescription("<owner=def>\nSharp."), QString("<owner=def>\nSharp."));
}

void Area::takeCharacterRefusesADuplicateClaim()
{
    QVERIFY(m_area->takeCharacter(5));

    // The refused second claim leaves the take list exactly as it was.
    QVERIFY(!m_area->takeCharacter(5));
    QCOMPARE(m_area->charactersTaken(), QList<int>({5}));
}

void Area::releaseCharacterOfANeverTakenIdIsHarmless()
{
    m_area->takeCharacter(5);

    m_area->releaseCharacter(9);
    QCOMPARE(m_area->charactersTaken(), QList<int>({5}));
}

void Area::changeCharacterRefusesATakenTarget()
{
    m_area->addClient(6, 0);

    // Nobody may switch onto a taken character, not even from spectator.
    QVERIFY(!m_area->changeCharacter(-1, 6));
    QCOMPARE(m_area->charactersTaken(), QList<int>({6}));

    // Switching to spectator answers false too, yet the source is released
    // - the false return means "no character held", not "nothing happened".
    QVERIFY(!m_area->changeCharacter(6, -1));
    QVERIFY(m_area->charactersTaken().isEmpty());
}

void Area::removeOwnerOfANonOwnerChangesNoOwnership()
{
    m_area->addOwner(4);
    m_area->setLockState(akashi::Area::LockState::Locked);
    m_area->invite(9);
    QSignalSpy l_owners(m_area, &akashi::Area::ownersChanged);

    // Removing a non-owner is refused by name and mutates nothing - not
    // even their invitation.
    QCOMPARE(m_area->removeOwner(9), akashi::Area::OwnerRemoval::NotAnOwner);
    QCOMPARE(m_area->owners(), QList<int>({4}));
    QCOMPARE(m_area->lockState(), akashi::Area::LockState::Locked);
    QCOMPARE(l_owners.size(), 0);
    QVERIFY(m_area->invited().contains(9));

    // Uninviting a player nobody ever invited answers false.
    QVERIFY(!m_area->uninvite(12));
}

void Area::removeOwnerReportsWhetherTheAreaUnlockedItself()
{
    m_area->addOwner(4);
    m_area->addOwner(7);
    m_area->setLockState(akashi::Area::LockState::Locked);
    QSignalSpy l_locks(m_area, &akashi::Area::lockStateChanged);

    // With another owner left the removal is plain and the lock holds.
    QCOMPARE(m_area->removeOwner(4), akashi::Area::OwnerRemoval::Removed);
    QCOMPARE(m_area->owners(), QList<int>({7}));
    QCOMPARE(m_area->lockState(), akashi::Area::LockState::Locked);
    QCOMPARE(l_locks.size(), 0);

    // The last owner leaving frees the locked area, and says so.
    QCOMPARE(m_area->removeOwner(7), akashi::Area::OwnerRemoval::RemovedAndUnlocked);
    QCOMPARE(m_area->lockState(), akashi::Area::LockState::Free);
    QCOMPARE(l_locks.size(), 1);

    // With nothing locked the last owner's exit is a plain removal too.
    m_area->addOwner(4);
    QCOMPARE(m_area->removeOwner(4), akashi::Area::OwnerRemoval::Removed);
    QCOMPARE(l_locks.size(), 1);
}

void Area::evidenceEditsIgnoreIndexesOutOfBounds()
{
    m_area->appendEvidence({"Sword", "A sword.", "sword.png"});
    m_area->appendEvidence({"Photo", "A photo.", "photo.png"});

    // Negative and past-end indexes fall through every mutation unchanged.
    m_area->deleteEvidence(-1);
    m_area->deleteEvidence(2);
    m_area->replaceEvidence(-1, {"Fake", "Fake.", "fake.png"});
    m_area->replaceEvidence(2, {"Fake", "Fake.", "fake.png"});
    m_area->swapEvidence(0, 2);
    m_area->swapEvidence(-1, 1);
    m_area->setEvidenceOwnerToAll(-1);
    m_area->setEvidenceOwnerToAll(2);

    const QList<akashi::Evidence> l_evidence = m_area->evidence();
    QCOMPARE(l_evidence.size(), 2);
    QCOMPARE(l_evidence.at(0).name, QString("Sword"));
    QCOMPARE(l_evidence.at(0).description, QString("A sword."));
    QCOMPARE(l_evidence.at(1).name, QString("Photo"));
    QCOMPARE(l_evidence.at(1).description, QString("A photo."));
}

void Area::notecardsStoreClearAndHandOverOnce()
{
    QVERIFY(m_area->addNotecard("Phoenix", "Remember the locket"));

    // Writing again replaces the card.
    QVERIFY(m_area->addNotecard("Phoenix", "Check the safe"));

    // Only the null string clears; an empty card is stored as written.
    QVERIFY(m_area->addNotecard("Franziska", QStringLiteral("")));
    QVERIFY(!m_area->addNotecard("Phoenix", QString()));

    // Clearing a card that was never written is harmless.
    QVERIFY(!m_area->addNotecard("Edgeworth", QString()));

    // Reading hands the cards over and forgets them.
    const QStringList l_cards = m_area->notecards();
    QVERIFY(l_cards.contains("Franziska"));
    QVERIFY(!l_cards.contains("Phoenix"));
    QVERIFY(!l_cards.contains("Edgeworth"));
    QVERIFY(m_area->notecards().isEmpty());
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::Area)

#include "tst_area.moc"
