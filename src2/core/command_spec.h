#pragma once

#include <QString>
#include <QStringList>

namespace akashi {

struct CommandSpec
{
    QString name;
    QStringList aliases;
    QStringList permissions;
    int min_args = 0;
    QString usage;
    QString description;
    int sensitive_args_from = -1;
};

} // namespace akashi

