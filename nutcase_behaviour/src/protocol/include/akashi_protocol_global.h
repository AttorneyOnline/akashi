#pragma once

#include <QtCore/QtGlobal>

#if defined(AKASHI_PROTOCOL_LIBRARY)
#define AKASHI_PROTOCOL_EXPORT Q_DECL_EXPORT
#else
#define AKASHI_PROTOCOL_EXPORT Q_DECL_IMPORT
#endif
