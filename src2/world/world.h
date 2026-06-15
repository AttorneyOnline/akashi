#pragma once

#include "akashi_core_export.h"
#include "world/area.h"
#include "world/floor.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

class QSettings;

namespace akashi {

class FileSystemService;
class RuleRegistry;
class ServiceRegistry;

// The map itself: every floor and area, how they are built from config,
// saved back to it, and reshaped at runtime. Purely structural - clients,
// packets and area-update broadcasts are the server's business; it listens
// to areaBuilt() and applies the id mappings the reshaping methods return.
class AKASHI_CORE_EXPORT World : public QObject
{
    Q_OBJECT

  public:
    World(RuleRegistry *f_rules, ServiceRegistry *f_services, FileSystemService *f_filesystem,
          QSettings *f_areas_ini, QSettings *f_ambience_ini, QObject *parent = nullptr);

    // --- Access ---

    const QVector<Area *> &areas() const { return m_areas; }
    int areaCount() const { return m_areas.size(); }
    Area *areaById(int f_area_id) const;
    QString areaName(int f_area_id) const;
    QStringList areaNames() const { return m_area_names; }

    QVector<Floor> &floors() { return m_floors; }
    int floorCount() const { return m_floors.size(); }
    const Floor *floorById(int f_floor_id) const;
    Floor *floorById(int f_floor_id);
    const Floor *floorByName(const QString &f_name) const;
    int floorIdForArea(int f_area_id) const;
    QStringList floorNames() const;

    // The floor-wide music catalog every jukebox layers its customs on.
    Floor &defaultFloor() { return m_default_floor; }

    // --- Building and config ---

    // Assembles floors and areas from the areas config; f_rules_path is the
    // raw areas.json the rule declarations load from.
    void buildFromConfig(const QString &f_rules_path);

    // Replaces all "config"-owned rules on floors and areas with the
    // declarations in the given file.
    void applyConfigRules(const QString &f_path);

    // --- Saving ---

    std::optional<QString> save(const QString &f_path);
    std::optional<QString> saveFloor(int f_floor_id, const QString &f_path);

    // --- Reshaping. The returned mapping is old area id -> new id, -1 for
    // --- removed; the server moves its clients with it.

    int createArea(const QString &f_name, int f_floor_id);
    int createFloor(const QString &f_name);
    bool renameArea(int f_area_id, const QString &f_name);
    bool renameFloor(int f_floor_id, const QString &f_name);
    std::optional<QString> removeArea(int f_area_id, QVector<int> &f_mapping);
    std::optional<QString> removeFloor(int f_floor_id, QVector<int> &f_mapping);

    // Loads a floor file: a same-named floor is replaced in place, an
    // unknown one becomes a new floor.
    std::optional<QString> loadFloorFile(const QString &f_name, const QString &f_path,
                                         int &f_floor_id, QVector<int> &f_mapping);

    // Tears every floor and area down and rebuilds from config. The caller
    // re-places its clients afterwards.
    void rebuild(const QString &f_rules_path);

  Q_SIGNALS:
    // A new area exists; the server wires its jukebox packets and the
    // area-update broadcaster here.
    void areaBuilt(akashi::Area *f_area);

    // Fires before an area is deleted so nothing keeps a pointer to it.
    void areaAboutToBeRemoved(akashi::Area *f_area);

  private:
    Area *buildArea(int f_area_id, const QString &f_name, int f_floor_id,
                    QSettings *f_settings = nullptr, const QString &f_settings_group = {});
    void applyDefaultFloorRules(Floor &f_floor);

    // Deletes the given areas and renumbers the survivors; returns the
    // old-to-new id mapping.
    QVector<int> compactAreas(QVector<int> f_removed_ids);

    RuleRegistry *m_rules;
    ServiceRegistry *m_services;
    FileSystemService *m_filesystem;
    QSettings *m_areas_ini;
    QSettings *m_ambience_ini;

    QVector<Area *> m_areas;
    QStringList m_area_names;
    QVector<Floor> m_floors;
    Floor m_default_floor;
};

} // namespace akashi
