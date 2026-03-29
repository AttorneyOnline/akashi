#pragma once

#include <QtCore/QtGlobal>

#if defined(AKASHI_ADDON_LIBRARY)
#define AKASHI_ADDON_EXPORT Q_DECL_EXPORT
#else
#define AKASHI_ADDON_EXPORT Q_DECL_IMPORT
#endif
