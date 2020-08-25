#ifndef AREA_DATA_H
#define AREA_DATA_H

#include <QMap>
#include <QString>

class AreaData {
  public:
    AreaData(QStringList characters);

    QString name;
    QMap<QString, bool> characters_taken;
    int player_count;
};

#endif // AREA_DATA_H
