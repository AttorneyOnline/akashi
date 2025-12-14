#pragma once

#include <QRandomGenerator>
#include <QString>
#include <QStringList>

namespace SoftwareInfo
{

inline const QStringList SPLASH_MESSAGES = {
    "90% cuter than competing brands."
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

 constexpr const char *BUILD_DATE = "2025-12-14 15:52:57";
 constexpr const char *BUILD_TYPE = "Release";

 constexpr const char *GIT_COMMIT = "b5ce446";
 constexpr const char *GIT_BRANCH = "service-checkinbranch";
}
