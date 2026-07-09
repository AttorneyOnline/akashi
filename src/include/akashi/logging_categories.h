#pragma once

#include "akashi_core_export.h"

#include <QLoggingCategory>

// The logging categories console messages file under, so owners can turn
// whole subsystems up or down without touching code. Qt reads the rules
// from the QT_LOGGING_RULES environment variable, for example:
//   QT_LOGGING_RULES="akashi.scripting.info=false;akashi.plugins.debug=true"
// Every log statement names a category; the bare qInfo/qWarning forms are
// reserved for code that cannot see this header.
AKASHI_CORE_EXPORT const QLoggingCategory &akashiServer();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiNet();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiDiscord();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiConfig();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiCommands();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiDb();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiLog();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiPlugins();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiScripting();
AKASHI_CORE_EXPORT const QLoggingCategory &akashiConsole();
