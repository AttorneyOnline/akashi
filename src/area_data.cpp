#include "include/area_data.h"

AreaData::AreaData(QStringList characters)
{
    for(QString cur_char : characters)
    {
        characters_taken.insert(cur_char, false);
    }
}
