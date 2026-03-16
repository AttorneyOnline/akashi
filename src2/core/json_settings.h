#ifndef JSON_SETTINGS_H
#define JSON_SETTINGS_H

#include "akashi_core_export.h"

#include <QSettings>

// QSettings support for JSON config files.
class AKASHI_CORE_EXPORT JsonSettings
{
  public:
    // The QSettings format for JSON files, registered on first use.
    static QSettings::Format format();

  private:
    static bool readJsonFile(QIODevice &device, QSettings::SettingsMap &map);
    static bool writeJsonFile(QIODevice &device, const QSettings::SettingsMap &map);
};

#endif // JSON_SETTINGS_H
