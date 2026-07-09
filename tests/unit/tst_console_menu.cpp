// AI-generated: written by Claude.
#include "core/console_menu.h"

#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_ConsoleMenu : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void actionsClearVtScreens();
    void nonVtTerminalsKeepAppending();

  private:
    // The menu is driven through its sink; the tasks view needs no server.
    ConsoleMenu *makeMenu(QByteArray *f_capture, bool f_vt);
};

ConsoleMenu *tst_ConsoleMenu::makeMenu(QByteArray *f_capture, bool f_vt)
{
    auto *l_menu = new ConsoleMenu(nullptr, this);
    l_menu->setSink([f_capture](const QByteArray &f_bytes) { f_capture->append(f_bytes); });
    // VT sequences only exist together with the interactive repaint mode.
    l_menu->setInteractive(f_vt, f_vt);
    l_menu->registerAction(QStringLiteral("wave"), [l_menu] { l_menu->printOut(QStringLiteral("hello from the task")); });
    l_menu->show();
    return l_menu;
}

void tst_ConsoleMenu::actionsClearVtScreens()
{
    QByteArray l_capture;
    ConsoleMenu *l_menu = makeMenu(&l_capture, true);

    // Opening the tasks view clears first, so only the new view shows.
    l_capture.clear();
    l_menu->handleLine(QStringLiteral("4"));
    QVERIFY(l_capture.contains("\x1b[2J"));
    QVERIFY(l_capture.contains("wave"));

    // Running a task clears before its output lands.
    l_capture.clear();
    l_menu->handleLine(QStringLiteral("1"));
    const int l_clear_at = l_capture.indexOf("\x1b[2J");
    const int l_output_at = l_capture.indexOf("hello from the task");
    QVERIFY(l_clear_at >= 0);
    QVERIFY(l_output_at > l_clear_at);

    // Backing out of the view clears too.
    l_capture.clear();
    l_menu->handleLine(QStringLiteral("0"));
    QVERIFY(l_capture.contains("\x1b[2J"));
}

void tst_ConsoleMenu::nonVtTerminalsKeepAppending()
{
    QByteArray l_capture;
    ConsoleMenu *l_menu = makeMenu(&l_capture, false);

    l_menu->handleLine(QStringLiteral("4"));
    l_menu->handleLine(QStringLiteral("1"));
    l_menu->handleLine(QStringLiteral("0"));
    QVERIFY(!l_capture.contains("\x1b[2J"));
    QVERIFY(l_capture.contains("hello from the task"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_ConsoleMenu)

#include "tst_console_menu.moc"
