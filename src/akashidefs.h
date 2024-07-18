#pragma once

#include <QString>
#include <QUrl>

namespace akashi {

struct PublisherInfo
{
    QString servername;
    std::optional<QString> ip;
    QString description;
    QString serverlist;
    int port;
    int players;
    bool enabled;
    bool rp_enabled; //< Reverse Proxy solution
};
}
