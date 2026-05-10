// Utility functions that non-command code still calls. The command-specific
// helpers (diceThrower, areaTimer, parseTime, reprimand, sendNotice) have
// migrated to their respective src2/commands/*_commands.cpp files.
#include "aoclient.h"

#include <QRandomGenerator>

void AOClient::cmdDefault(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    sendServerMessage("Invalid command.");
    return;
}

int AOClient::genRand(int min, int max)
{
    return QRandomGenerator::system()->bounded(min, max + 1);
}
