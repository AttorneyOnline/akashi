// AI-generated: written by Claude.
#include <QTest>

#include "akashi/config_store.h"
#include "config_manager.h"
#include "fake_packet_context.h"
#include "proto/chat.h"
#include "proto/packet_codec.h"
#include "proto/packet_registry.h"

namespace tests {
namespace unittests {

using namespace akashi;

class tst_Chat : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();

    void oocSanitizesTheName();
    void oocRefusesMutedEmptyAndImpersonation();
    void oocPasswordGoesToTheLoginPrompt();
    void oocSlashRunsACommand();
    void oocBroadcastsPlainMessages();
    void evidenceDeleteChecksAccessAndBounds();
    void evidenceEditChecksAccessAndBounds();
    void evidenceEditTagsHiddenCmEvidence();
    void casingPreferencesNeedFiveNumbers();

  private:
    // Runs a packet the way the dispatcher does: resolve, decode, handle.
    void run(const Packet &f_packet, FakeContext &f_context);

    PacketRegistry m_handlers;
    PacketCodecRegistry m_codecs;
};

void tst_Chat::initTestCase()
{
    QVERIFY(ConfigManager::setStore(new akashi::ConfigStore("config", this)));
    registerChatPackets(m_handlers, m_codecs);
}

void tst_Chat::run(const Packet &f_packet, FakeContext &f_context)
{
    const auto l_spec = m_handlers.spec(f_packet.header());
    QVERIFY(l_spec.has_value());
    QVERIFY(f_packet.fieldCount() >= l_spec->min_args);

    const auto l_codec = m_codecs.resolve(f_context.profile()).codecFor(f_packet.header());
    QVERIFY(l_codec);
    const auto l_message = l_codec->decode(f_packet);
    QVERIFY(l_message);
    m_handlers.handler(f_packet.header())->handle(*l_message, f_context);
}

void tst_Chat::oocSanitizesTheName()
{
    FakeContext l_context;
    run(Packet("CT", {"na[m]e#$%&{}", "hello"}), l_context);

    // Brackets, braces and escape characters leave the name.
    QCOMPARE(l_context.ooc_name, QString("name"));
    QCOMPARE(l_context.ooc_broadcasts, QStringList({"hello"}));
}

void tst_Chat::oocRefusesMutedEmptyAndImpersonation()
{
    FakeContext l_muted;
    l_muted.ooc_chat_allowed = false;
    run(Packet("CT", {"someone", "hello"}), l_muted);
    QVERIFY(l_muted.ooc_broadcasts.isEmpty());
    QCOMPARE(l_muted.calls, QStringList({"message:You are OOC muted, and cannot speak."}));

    FakeContext l_nameless;
    run(Packet("CT", {"[]", "hello"}), l_nameless);
    QVERIFY(l_nameless.ooc_broadcasts.isEmpty());

    FakeContext l_impersonator;
    run(Packet("CT", {ConfigManager::serverNickname(), "hello"}), l_impersonator);
    QVERIFY(l_impersonator.ooc_broadcasts.isEmpty());

    FakeContext l_longname;
    run(Packet("CT", {QString(31, 'a'), "hello"}), l_longname);
    QVERIFY(l_longname.ooc_broadcasts.isEmpty());
    QVERIFY(l_longname.calls.last().startsWith("message:Your name is too long!"));
}

void tst_Chat::oocPasswordGoesToTheLoginPrompt()
{
    FakeContext l_context;
    l_context.in_login_prompt = true;
    run(Packet("CT", {"someone", "hunter2"}), l_context);

    QCOMPARE(l_context.login_attempts, QStringList({"hunter2"}));
    QVERIFY(l_context.ooc_broadcasts.isEmpty());
    QVERIFY(l_context.commands_run.isEmpty());
}

void tst_Chat::oocSlashRunsACommand()
{
    FakeContext l_context;
    run(Packet("CT", {"someone", "/BanInfo 1  hdid"}), l_context);

    // The command name is lowercased and the extra spaces collapse.
    QCOMPARE(l_context.commands_run, QStringList({"baninfo|1,hdid"}));
    QVERIFY(l_context.ooc_broadcasts.isEmpty());
}

void tst_Chat::oocBroadcastsPlainMessages()
{
    FakeContext l_context;
    run(Packet("CT", {"someone", "an ordinary chat line"}), l_context);
    QCOMPARE(l_context.ooc_broadcasts, QStringList({"an ordinary chat line"}));

    FakeContext l_empty;
    run(Packet("CT", {"someone", ""}), l_empty);
    QVERIFY(l_empty.ooc_broadcasts.isEmpty());
}

void tst_Chat::evidenceDeleteChecksAccessAndBounds()
{
    FakeContext l_denied;
    l_denied.evidence_access = false;
    l_denied.evidence_total = 3;
    run(Packet("DE", {"1"}), l_denied);
    QVERIFY(l_denied.calls.isEmpty());

    FakeContext l_context;
    l_context.evidence_total = 3;
    run(Packet("DE", {"1"}), l_context);
    QCOMPARE(l_context.deleted_evidence, QList<int>({1}));
    // The delete happens before the area sees the fresh list.
    QCOMPARE(l_context.calls, QStringList({"deleteEvidence", "sendEvidenceList"}));

    // Out of range or unreadable indexes still refresh the list.
    FakeContext l_out_of_range;
    l_out_of_range.evidence_total = 3;
    run(Packet("DE", {"7"}), l_out_of_range);
    QVERIFY(l_out_of_range.deleted_evidence.isEmpty());
    QCOMPARE(l_out_of_range.calls, QStringList({"sendEvidenceList"}));

    FakeContext l_garbage;
    l_garbage.evidence_total = 3;
    run(Packet("DE", {"first"}), l_garbage);
    QVERIFY(l_garbage.deleted_evidence.isEmpty());
    QCOMPARE(l_garbage.calls, QStringList({"sendEvidenceList"}));
}

void tst_Chat::evidenceEditChecksAccessAndBounds()
{
    FakeContext l_denied;
    l_denied.evidence_access = false;
    l_denied.evidence_total = 3;
    run(Packet("EE", {"1", "Knife", "Sharp.", "knife.png"}), l_denied);
    QVERIFY(l_denied.calls.isEmpty());

    FakeContext l_out_of_range;
    l_out_of_range.evidence_total = 3;
    run(Packet("EE", {"5", "Knife", "Sharp.", "knife.png"}), l_out_of_range);
    QVERIFY(l_out_of_range.calls.isEmpty());

    FakeContext l_context;
    l_context.evidence_total = 3;
    run(Packet("EE", {"1", "Knife", "Sharp.", "knife.png"}), l_context);
    QCOMPARE(l_context.replaced_evidence, QStringList({"1", "Knife", "Sharp.", "knife.png"}));
    QCOMPARE(l_context.calls, QStringList({"replaceEvidence", "sendEvidenceList"}));
}

void tst_Chat::evidenceEditTagsHiddenCmEvidence()
{
    FakeContext l_untagged;
    l_untagged.evidence_total = 3;
    l_untagged.evidence_hidden_cm = true;
    run(Packet("EE", {"1", "Knife", "Sharp.", "knife.png"}), l_untagged);
    QCOMPARE(l_untagged.replaced_evidence.at(2), QString("<owner=all>\nSharp."));

    // An existing owner tag stays untouched.
    FakeContext l_tagged;
    l_tagged.evidence_total = 3;
    l_tagged.evidence_hidden_cm = true;
    run(Packet("EE", {"1", "Knife", "<owner=def>\nSharp.", "knife.png"}), l_tagged);
    QCOMPARE(l_tagged.replaced_evidence.at(2), QString("<owner=def>\nSharp."));
}

void tst_Chat::casingPreferencesNeedFiveNumbers()
{
    FakeContext l_context;
    run(Packet("SETCASE", {"", "", "1", "0", "1", "0", "1"}), l_context);
    QCOMPARE(l_context.casing_preferences, QList<bool>({true, false, true, false, true}));

    FakeContext l_garbage;
    run(Packet("SETCASE", {"", "", "1", "yes", "1", "0", "1"}), l_garbage);
    QVERIFY(l_garbage.casing_preferences.isEmpty());
    QVERIFY(l_garbage.calls.isEmpty());
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_Chat)

#include "tst_chat.moc"
