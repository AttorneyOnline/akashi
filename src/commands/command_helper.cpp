#include "aoclient.h"

#include <QRandomGenerator>

int AOClient::genRand(int min, int max)
{
    return QRandomGenerator::system()->bounded(min, max + 1);
}
