#ifndef CORE_COMMAND_SPEC_H
#define CORE_COMMAND_SPEC_H

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

#endif // CORE_COMMAND_SPEC_H
