#pragma once

#include <QString>

struct ModcallPayload
{
    int client_id = -1;
    QString name;
    QString area_name;
    QString reason;
};

struct BanPayload
{
    int ban_id = -1;
    QString moderator;
    QString target_ipid;
    QString duration;
    QString reason;
};
