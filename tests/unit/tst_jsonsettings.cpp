// AI-generated: written by Claude.
#include "core/json_settings.h"

#include <QTemporaryDir>
#include <QTest>

namespace tests {
namespace unittests {

class tst_JsonSettings : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void writeAndReadBack();
    void readHandwrittenFile();
    void rejectInvalidFile();
};

void tst_JsonSettings::writeAndReadBack()
{
    QTemporaryDir dir;
    const QString path = dir.path() + "/config.json";

    {
        QSettings settings(path, JsonSettings::format());
        settings.setValue("Options/port", 27016);
        settings.setValue("Options/server_name", "An Unnamed Server");
        settings.setValue("Options/advertise", false);
        settings.setValue("toplevel", "value");
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
    }

    QSettings settings(path, JsonSettings::format());
    QCOMPARE(settings.value("Options/port").toInt(), 27016);
    QCOMPARE(settings.value("Options/server_name").toString(), "An Unnamed Server");
    QCOMPARE(settings.value("Options/advertise").toBool(), false);
    QCOMPARE(settings.value("toplevel").toString(), "value");
}

void tst_JsonSettings::readHandwrittenFile()
{
    QTemporaryDir dir;
    const QString path = dir.path() + "/config.json";

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"Options": {"port": 5000, "nested": {"deep": true}}, "list": ["a", "b"]})");
    file.close();

    QSettings settings(path, JsonSettings::format());
    QCOMPARE(settings.status(), QSettings::NoError);
    QCOMPARE(settings.value("Options/port").toInt(), 5000);
    QCOMPARE(settings.value("Options/nested/deep").toBool(), true);
    QCOMPARE(settings.value("list").toStringList(), QStringList({"a", "b"}));
}

void tst_JsonSettings::rejectInvalidFile()
{
    QTemporaryDir dir;
    const QString path = dir.path() + "/config.json";

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not json");
    file.close();

    QSettings settings(path, JsonSettings::format());
    QCOMPARE(settings.status(), QSettings::FormatError);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_JsonSettings)

#include "tst_jsonsettings.moc"
