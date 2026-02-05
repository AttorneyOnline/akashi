#pragma once

#include <QRandomGenerator>
#include <QString>
#include <QStringList>

namespace SoftwareInfo
{

inline const QStringList SPLASH_MESSAGES = {
    "90% cuter than competing brands.",
    "Powered by HVAC technology."
};

inline QString getRandomSplash() {
    int index = QRandomGenerator::global()->bounded(SPLASH_MESSAGES.size());
    return SPLASH_MESSAGES[index];
}

 constexpr const char *NAME = "akashi";
 constexpr const char *DISPLAY_NAME = "Akashi";
 constexpr const char *CODENAME = "kiwi";

 constexpr int VERSION_MAJOR = 2;
 constexpr int VERSION_MINOR = 0;
 constexpr const char *VERSION = "2.0";

 constexpr const char *BUILD_DATE = "2026-02-05 08:52:45";
 constexpr const char *BUILD_TYPE = "Debug";

 constexpr const char *GIT_COMMIT = "8f2f3fe";
 constexpr const char *GIT_BRANCH = "service-checkinbranch";
}
