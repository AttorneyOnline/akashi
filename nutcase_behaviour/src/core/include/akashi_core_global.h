#pragma once

#include <QtCore/QtGlobal>

#if defined(AKASHI_CORE_LIBRARY)
#define AKASHI_CORE_EXPORT Q_DECL_EXPORT
#else
#define AKASHI_CORE_EXPORT Q_DECL_IMPORT
#endif
