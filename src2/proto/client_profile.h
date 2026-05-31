#pragma once

#include "akashi_core_export.h"

#include <QSet>
#include <QString>

namespace akashi {

// The client version parsed from the ID packet, for example 2.10.0.
struct ClientVersion
{
    int release = 0;
    int major = 0;
    int minor = 0;

    // True if this version is at least the given one.
    bool atLeast(int f_release, int f_major, int f_minor) const
    {
        if (release != f_release) {
            return release > f_release;
        }
        if (major != f_major) {
            return major > f_major;
        }
        return minor >= f_minor;
    }
};

// Everything known about a connection that decides which codecs it uses.
// Built once, after the client sends its ID packet.
struct AKASHI_CORE_EXPORT ClientProfile
{
    QString arch;           // "AO2", "webAO" or a fork identifier
    ClientVersion version;  // parsed from the ID packet
    QSet<QString> features; // the capabilities the client announced at connect time

    bool hasFeature(const QString &f_feature) const { return features.contains(f_feature); }
};

} // namespace akashi

