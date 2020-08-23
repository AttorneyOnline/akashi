#include "include/akashimain.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  QApplication::setApplicationName("akashi");
  QApplication::setApplicationVersion("0.0.1");

  QSettings config("config.ini", QSettings::IniFormat);
  config.beginGroup("Options");
  QString language = config.value("language", QLocale().bcp47Name()).toString();

  QTranslator qt_translator;
  qt_translator.load("qt_" + language,
                     QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  app.installTranslator(&qt_translator);

  QTranslator translator;
  translator.load("akashi_" + language, ":/resource/translation/");
  app.installTranslator(&translator);

  QCommandLineParser parser;
  parser.setApplicationDescription(
      app.translate("main", "A server for Attorney Online 2"));
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption headlessOption(
      QStringList() << "l"
                    << "headless",
      app.translate("main", "Run the server without a GUI"));
  parser.addOption(headlessOption);

  parser.process(app);
  bool headless = parser.isSet(headlessOption);

  AkashiMain w;
  if (!headless)
    w.show();

  return app.exec();
}
