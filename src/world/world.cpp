#include "world/world.h"

#include "akashi/filesystem_service.h"
#include "core/config_loading.h"
#include "core/json_settings.h"
#include "core/permission_registry.h"
#include "world/jukebox.h"
#include "world/rule_registry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>

namespace akashi {

World::World(RuleRegistry *f_rules, ServiceRegistry *f_services, FileSystemService *f_filesystem,
             QSettings *f_areas_ini, QSettings *f_ambience_ini, QObject *parent) :
    QObject(parent),
    m_rules(f_rules),
    m_services(f_services),
    m_filesystem(f_filesystem),
    m_areas_ini(f_areas_ini),
    m_ambience_ini(f_ambience_ini)
{
}

// --- Access ---

Area *World::areaById(int f_area_id) const
{
    if (f_area_id >= 0 && f_area_id < m_areas.size()) {
        return m_areas[f_area_id];
    }
    return nullptr;
}

QString World::areaName(int f_area_id) const
{
    if (f_area_id >= 0 && f_area_id < m_area_names.size()) {
        return m_area_names.at(f_area_id);
    }
    return {};
}

const Floor *World::floorById(int f_floor_id) const
{
    if (f_floor_id >= 0 && f_floor_id < m_floors.size()) {
        return &m_floors[f_floor_id];
    }
    return nullptr;
}

Floor *World::floorById(int f_floor_id)
{
    if (f_floor_id >= 0 && f_floor_id < m_floors.size()) {
        return &m_floors[f_floor_id];
    }
    return nullptr;
}

const Floor *World::floorByName(const QString &f_name) const
{
    for (const Floor &l_floor : m_floors) {
        if (l_floor.name.compare(f_name, Qt::CaseInsensitive) == 0) {
            return &l_floor;
        }
    }
    return nullptr;
}

int World::floorIdForArea(int f_area_id) const
{
    if (f_area_id >= 0 && f_area_id < m_areas.size()) {
        return m_areas[f_area_id]->floorId();
    }
    return 0;
}

QStringList World::floorNames() const
{
    QStringList l_names;
    for (const Floor &l_floor : m_floors) {
        l_names.append(l_floor.name);
    }
    return l_names;
}

// --- Building and config ---

Area *World::buildArea(int f_area_id, const QString &f_name, int f_floor_id, QSettings *f_settings, const QString &f_settings_group)
{
    Floor *l_floor = floorById(f_floor_id);
    int l_x_on_floor = l_floor->area_ids.size();
    l_floor->area_ids.append(f_area_id);

    // Floor files number their sections locally, so their group name is
    // not the area's global id.
    const QString l_group = f_settings_group.isEmpty() ? QString::number(f_area_id) + ":" + f_name : f_settings_group;
    Area *l_area = new Area(l_group, f_area_id, f_floor_id, l_x_on_floor, f_settings ? f_settings : m_areas_ini, m_ambience_ini, this);
    m_areas.insert(f_area_id, l_area);
    l_area->jukebox()->setFloorCatalog(&m_default_floor);

    Q_EMIT areaBuilt(l_area);
    return l_area;
}

void World::applyDefaultFloorRules(Floor &f_floor)
{
    // The gates every floor enforces out of the box.
    struct GateDefault
    {
        QString event;
        QString action;
        QVariantMap args;
    };
    const QVector<GateDefault> l_gate_defaults = {
        {AreaEvents::PlayerJoined, QStringLiteral("check_lock"), {}},
        {AreaEvents::IcMessageSent, QStringLiteral("check_blankposting"), {}},
        {AreaEvents::IcMessageSent, QStringLiteral("check_iniswap"), {}},
        {AreaEvents::MusicChanged, QStringLiteral("check_setting"),
         {{QStringLiteral("setting"), QStringLiteral("music")},
          {QStringLiteral("bypass"), permission::gamemaster},
          {QStringLiteral("message"), QStringLiteral("Music is disabled in this area.")}}},
        {AreaEvents::EvidenceAdded, QStringLiteral("check_evidence_access"), {}},
        {AreaEvents::EvidenceRemoved, QStringLiteral("check_evidence_access"), {}},
        {AreaEvents::EvidenceEdited, QStringLiteral("check_evidence_access"), {}},
        {AreaEvents::BackgroundChanged, QStringLiteral("check_background"), {}},
    };
    for (const GateDefault &l_gate : l_gate_defaults) {
        if (auto l_function = m_rules->buildBefore(l_gate.action, *m_services, l_gate.args)) {
            f_floor.before_rules.append({l_gate.event, l_gate.action, *l_function, QStringLiteral("core"), l_gate.args});
        }
    }

    // The actions that hand a joining player the area's state. The append
    // order here is the send order clients see.
    const QStringList l_join_defaults = {
        QStringLiteral("send_evidence_list"),
        QStringLiteral("send_penalties"),
        QStringLiteral("send_background"),
        QStringLiteral("send_timers"),
        QStringLiteral("send_song"),
        QStringLiteral("send_floor_areas"),
        QStringLiteral("send_area_description"),
        QStringLiteral("send_lock_notice"),
    };
    for (const QString &l_name : l_join_defaults) {
        if (auto l_action = m_rules->buildAfter(l_name, *m_services, {})) {
            f_floor.after_rules.append({AreaEvents::PlayerJoined, l_name, *l_action, QStringLiteral("core")});
        }
    }
}

void World::buildFromConfig(const QString &f_rules_path)
{
    // Assembles the area list, reading an optional floor assignment per area.
    // Sections without a numeric prefix (like "floors") are not areas.
    QStringList l_raw_names = m_areas_ini->childGroups();
    l_raw_names.erase(std::remove_if(l_raw_names.begin(), l_raw_names.end(), [](const QString &f_raw) {
                          bool l_is_area = false;
                          f_raw.split(":").first().toInt(&l_is_area);
                          return !l_is_area;
                      }),
                      l_raw_names.end());
    std::sort(l_raw_names.begin(), l_raw_names.end(), [](const QString &a, const QString &b) { return a.split(":")[0].toInt() < b.split(":")[0].toInt(); });

    QMap<QString, int> l_floor_name_to_id;
    QStringList l_area_floor_names;

    for (const QString &l_raw : qAsConst(l_raw_names)) {
        QStringList l_parts = l_raw.split(":");
        l_parts.removeFirst();
        m_area_names.append(l_parts.join(":"));

        m_areas_ini->beginGroup(l_raw);
        QString l_floor_name = m_areas_ini->value("floor", "Default").toString();
        m_areas_ini->endGroup();
        l_area_floor_names.append(l_floor_name);

        if (!l_floor_name_to_id.contains(l_floor_name)) {
            int l_id = l_floor_name_to_id.size();
            l_floor_name_to_id.insert(l_floor_name, l_id);
        }
    }

    m_floors.resize(l_floor_name_to_id.size());
    for (auto it = l_floor_name_to_id.constBegin(); it != l_floor_name_to_id.constEnd(); ++it) {
        m_floors[it.value()].id = it.value();
        m_floors[it.value()].name = it.key();
    }

    for (Floor &l_floor : m_floors) {
        applyDefaultFloorRules(l_floor);
    }

    for (int i = 0; i < m_area_names.length(); i++) {
        buildArea(i, m_area_names[i], l_floor_name_to_id.value(l_area_floor_names[i], 0));
    }

    // Rules declared in areas.json layer on top of the core defaults.
    applyConfigRules(f_rules_path);
}

void World::applyConfigRules(const QString &f_path)
{
    // Config rules are replaced wholesale. Core defaults and rules added at
    // runtime by plugins or commands stay untouched.
    auto l_sweep = [](auto &f_rules) {
        for (int i = f_rules.size() - 1; i >= 0; --i) {
            if (f_rules.at(i).owner_id == QStringLiteral("config"))
                f_rules.removeAt(i);
        }
    };
    for (Floor &l_floor : m_floors) {
        l_sweep(l_floor.before_rules);
        l_sweep(l_floor.after_rules);
    }
    for (Area *l_area : qAsConst(m_areas)) {
        l_sweep(l_area->beforeRules());
        l_sweep(l_area->afterRules());
    }

    const config::AreaRulesConfig l_config = config::loadAreaRules(f_path);

    auto l_apply = [this](const config::RuleDeclaration &f_declaration,
                          QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after) {
        if (f_declaration.phase == RulePhase::Before) {
            if (auto l_function = m_rules->buildBefore(f_declaration.action, *m_services, f_declaration.args)) {
                f_before.append({f_declaration.event, f_declaration.action, *l_function, QStringLiteral("config"), f_declaration.args});
                return;
            }
        }
        else if (auto l_function = m_rules->buildAfter(f_declaration.action, *m_services, f_declaration.args)) {
            f_after.append({f_declaration.event, f_declaration.action, *l_function, QStringLiteral("config"), f_declaration.args});
            return;
        }
        qWarning() << "Skipping rule for event" << f_declaration.event << "-" << f_declaration.action
                   << "is not a registered action of that phase.";
    };

    for (auto it = l_config.floor_rules.constBegin(); it != l_config.floor_rules.constEnd(); ++it) {
        Floor *l_floor = nullptr;
        for (Floor &l_candidate : m_floors) {
            if (l_candidate.name == it.key())
                l_floor = &l_candidate;
        }
        if (!l_floor) {
            qWarning() << "areas.json declares rules for unknown floor" << it.key();
            continue;
        }
        for (const config::RuleDeclaration &l_declaration : it.value())
            l_apply(l_declaration, l_floor->before_rules, l_floor->after_rules);
    }

    for (auto it = l_config.area_rules.constBegin(); it != l_config.area_rules.constEnd(); ++it) {
        Area *l_area = areaById(it.key());
        if (!l_area) {
            qWarning() << "areas.json declares rules for unknown area index" << it.key();
            continue;
        }
        for (const config::RuleDeclaration &l_declaration : it.value())
            l_apply(l_declaration, l_area->beforeRules(), l_area->afterRules());
    }
}

// --- Saving ---

// The rules an area or floor carries that belong in the config file: the
// ones config put there and the ones added by command.
static QJsonObject rulesToJson(const QVector<BeforeRuleEntry> &f_before, const QVector<AfterRuleEntry> &f_after)
{
    QJsonObject l_result;
    const auto l_add = [&l_result](const QString &f_event, const QString &f_bucket, const QString &f_action, const QVariantMap &f_args) {
        QJsonObject l_event = l_result[f_event].toObject();
        QJsonArray l_bucket_rules = l_event[f_bucket].toArray();
        QJsonObject l_entry;
        l_entry[QStringLiteral("action")] = f_action;
        for (auto it = f_args.constBegin(); it != f_args.constEnd(); ++it) {
            l_entry[it.key()] = QJsonValue::fromVariant(it.value());
        }
        l_bucket_rules.append(l_entry);
        l_event[f_bucket] = l_bucket_rules;
        l_result[f_event] = l_event;
    };
    const auto l_saved = [](const QString &f_owner) {
        return f_owner == QStringLiteral("config") || f_owner == QStringLiteral("command");
    };
    for (const BeforeRuleEntry &l_entry : f_before) {
        if (l_saved(l_entry.owner_id))
            l_add(l_entry.event, QStringLiteral("before"), l_entry.action, l_entry.args);
    }
    for (const AfterRuleEntry &l_entry : f_after) {
        if (l_saved(l_entry.owner_id))
            l_add(l_entry.event, QStringLiteral("after"), l_entry.action, l_entry.args);
    }
    return l_result;
}

// One area's settings and savable rules, in the shape areas.json reads.
static QJsonObject areaToJson(Area *f_area, const QString &f_floor_name)
{
    const auto l_bool = [](bool f_value) { return f_value ? QStringLiteral("true") : QStringLiteral("false"); };
    const auto l_access = [](EvidenceStore::Access f_access) {
        switch (f_access) {
        case EvidenceStore::Access::Mod:
            return QStringLiteral("mod");
        case EvidenceStore::Access::Cm:
            return QStringLiteral("cm");
        case EvidenceStore::Access::HiddenCm:
            return QStringLiteral("hidden_cm");
        case EvidenceStore::Access::FreeForAll:
        default:
            return QStringLiteral("ffa");
        }
    };

    QJsonObject l_entry;
    l_entry[QStringLiteral("floor")] = f_floor_name;
    l_entry[QStringLiteral("background")] = f_area->background();
    l_entry[QStringLiteral("bg_locked")] = l_bool(f_area->isBgLocked());
    l_entry[QStringLiteral("protected_area")] = l_bool(f_area->isProtected());
    l_entry[QStringLiteral("iniswap_allowed")] = l_bool(f_area->isIniswapAllowed());
    l_entry[QStringLiteral("evidence_mod")] = l_access(f_area->evidenceAccess());
    l_entry[QStringLiteral("blankposting_allowed")] = l_bool(f_area->isBlankpostingAllowed());
    l_entry[QStringLiteral("force_immediate")] = l_bool(f_area->forceImmediate());
    l_entry[QStringLiteral("toggle_music")] = l_bool(f_area->isMusicAllowed());
    l_entry[QStringLiteral("shownames_allowed")] = l_bool(f_area->isShownameAllowed());
    l_entry[QStringLiteral("ignore_bglist")] = l_bool(f_area->ignoreBgList());
    l_entry[QStringLiteral("jukebox_enabled")] = l_bool(f_area->isJukeboxEnabled());
    l_entry[QStringLiteral("playcmd_enabled")] = l_bool(f_area->isPlayEnabled());
    l_entry[QStringLiteral("wtce_enabled")] = l_bool(f_area->isWtceAllowed());
    l_entry[QStringLiteral("shouts_enabled")] = l_bool(f_area->isShoutAllowed());
    if (!f_area->areaMessage().isEmpty()) {
        l_entry[QStringLiteral("area_message")] = f_area->areaMessage();
        l_entry[QStringLiteral("send_area_message_on_join")] = l_bool(f_area->sendAreaMessageOnJoin());
    }
    const QJsonObject l_rules = rulesToJson(f_area->beforeRules(), f_area->afterRules());
    if (!l_rules.isEmpty()) {
        l_entry[QStringLiteral("rules")] = l_rules;
    }
    return l_entry;
}

std::optional<QString> World::save(const QString &f_path)
{
    QJsonObject l_root;

    QJsonObject l_floors;
    for (const Floor &l_floor : qAsConst(m_floors)) {
        const QJsonObject l_rules = rulesToJson(l_floor.before_rules, l_floor.after_rules);
        if (!l_rules.isEmpty()) {
            QJsonObject l_entry;
            l_entry[QStringLiteral("rules")] = l_rules;
            l_floors[l_floor.name] = l_entry;
        }
    }
    if (!l_floors.isEmpty()) {
        l_root[QStringLiteral("floors")] = l_floors;
    }

    for (int i = 0; i < m_areas.size(); ++i) {
        l_root[QString::number(i) + ":" + m_area_names[i]] = areaToJson(m_areas[i], floorById(m_areas[i]->floorId())->name);
    }

    return m_filesystem->writeFile(f_path, QJsonDocument(l_root).toJson(QJsonDocument::Indented));
}

std::optional<QString> World::saveFloor(int f_floor_id, const QString &f_path)
{
    const Floor *l_floor = floorById(f_floor_id);
    if (!l_floor) {
        return QStringLiteral("There is no floor with that ID.");
    }

    QJsonObject l_root;
    QJsonObject l_floors;
    QJsonObject l_floor_entry;
    const QJsonObject l_floor_rules = rulesToJson(l_floor->before_rules, l_floor->after_rules);
    if (!l_floor_rules.isEmpty()) {
        l_floor_entry[QStringLiteral("rules")] = l_floor_rules;
    }
    l_floors[l_floor->name] = l_floor_entry;
    l_root[QStringLiteral("floors")] = l_floors;

    // Sections are numbered locally: the floor file stands on its own.
    for (int l_x = 0; l_x < l_floor->area_ids.size(); ++l_x) {
        const int l_area_id = l_floor->area_ids[l_x];
        l_root[QString::number(l_x) + ":" + m_area_names[l_area_id]] = areaToJson(m_areas[l_area_id], l_floor->name);
    }

    return m_filesystem->writeFile(f_path, QJsonDocument(l_root).toJson(QJsonDocument::Indented));
}

// --- Reshaping ---

int World::createArea(const QString &f_name, int f_floor_id)
{
    const QString l_name = f_name.trimmed();
    if (l_name.isEmpty() || !floorById(f_floor_id)) {
        return -1;
    }
    const int l_area_id = m_areas.size();
    m_area_names.append(l_name);
    buildArea(l_area_id, l_name, f_floor_id);
    return l_area_id;
}

int World::createFloor(const QString &f_name)
{
    const QString l_name = f_name.trimmed();
    if (l_name.isEmpty() || floorByName(l_name)) {
        return -1;
    }
    Floor l_floor;
    l_floor.id = m_floors.size();
    l_floor.name = l_name;
    m_floors.append(l_floor);
    applyDefaultFloorRules(m_floors.last());
    // A floor without an area cannot be entered.
    createArea(QStringLiteral("Unnamed Area"), l_floor.id);
    return l_floor.id;
}

bool World::renameArea(int f_area_id, const QString &f_name)
{
    const QString l_name = f_name.trimmed();
    Area *l_area = areaById(f_area_id);
    if (!l_area || l_name.isEmpty()) {
        return false;
    }
    m_area_names[f_area_id] = l_name;
    l_area->setName(l_name);
    return true;
}

bool World::renameFloor(int f_floor_id, const QString &f_name)
{
    const QString l_name = f_name.trimmed();
    Floor *l_floor = floorById(f_floor_id);
    if (!l_floor || l_name.isEmpty() || floorByName(l_name)) {
        return false;
    }
    l_floor->name = l_name;
    return true;
}

QVector<int> World::compactAreas(QVector<int> f_removed_ids)
{
    std::sort(f_removed_ids.begin(), f_removed_ids.end(), [](int a, int b) { return a > b; });

    // Where every old id lands after the removal.
    QVector<int> l_mapping(m_areas.size());
    for (int l_old = 0; l_old < l_mapping.size(); ++l_old) {
        int l_shift = 0;
        bool l_removed = false;
        for (int l_id : qAsConst(f_removed_ids)) {
            if (l_old == l_id) {
                l_removed = true;
            }
            else if (l_id < l_old) {
                ++l_shift;
            }
        }
        l_mapping[l_old] = l_removed ? -1 : l_old - l_shift;
    }

    for (int l_id : qAsConst(f_removed_ids)) {
        Area *l_area = m_areas[l_id];
        Q_EMIT areaAboutToBeRemoved(l_area);
        m_areas.removeAt(l_id);
        m_area_names.removeAt(l_id);
        l_area->deleteLater();
    }

    // Rebuild the floors' area lists and each area's identity in one sweep.
    for (Floor &l_floor : m_floors) {
        QVector<int> l_survivors;
        for (int l_old : qAsConst(l_floor.area_ids)) {
            if (l_mapping[l_old] >= 0)
                l_survivors.append(l_mapping[l_old]);
        }
        l_floor.area_ids = l_survivors;
        for (int l_x = 0; l_x < l_survivors.size(); ++l_x) {
            m_areas[l_survivors[l_x]]->renumber(l_survivors[l_x], l_floor.id, l_x);
        }
    }
    return l_mapping;
}

std::optional<QString> World::removeArea(int f_area_id, QVector<int> &f_mapping)
{
    Area *l_area = areaById(f_area_id);
    if (!l_area) {
        return QStringLiteral("There is no area with that ID.");
    }
    if (l_area->playerCount() > 0) {
        return QStringLiteral("That area is not empty.");
    }
    Floor *l_floor = floorById(l_area->floorId());
    if (l_floor->area_ids.size() <= 1) {
        return QStringLiteral("A floor needs at least one area; remove the floor instead.");
    }
    f_mapping = compactAreas({f_area_id});
    return std::nullopt;
}

std::optional<QString> World::removeFloor(int f_floor_id, QVector<int> &f_mapping)
{
    Floor *l_floor = floorById(f_floor_id);
    if (!l_floor) {
        return QStringLiteral("There is no floor with that ID.");
    }
    if (m_floors.size() <= 1) {
        return QStringLiteral("The last floor cannot be removed.");
    }
    for (int l_area_id : qAsConst(l_floor->area_ids)) {
        if (m_areas[l_area_id]->playerCount() > 0) {
            return QStringLiteral("Not every area on that floor is empty.");
        }
    }

    f_mapping = compactAreas(l_floor->area_ids);
    m_floors.removeAt(f_floor_id);
    for (int i = f_floor_id; i < m_floors.size(); ++i) {
        m_floors[i].id = i;
    }
    // Areas on the shifted floors take their floor's new id.
    for (Floor &l_survivor : m_floors) {
        for (int l_x = 0; l_x < l_survivor.area_ids.size(); ++l_x) {
            m_areas[l_survivor.area_ids[l_x]]->renumber(l_survivor.area_ids[l_x], l_survivor.id, l_x);
        }
    }
    return std::nullopt;
}

std::optional<QString> World::loadFloorFile(const QString &f_name, const QString &f_path,
                                            int &f_floor_id, QVector<int> &f_mapping)
{
    // Rules come from the raw file; settings go through the same QSettings
    // path the startup loader uses.
    const config::AreaRulesConfig l_rule_config = config::loadAreaRules(f_path);
    QSettings l_file_settings(f_path, JsonSettings::format());
    QStringList l_raw_names = l_file_settings.childGroups();
    l_raw_names.erase(std::remove_if(l_raw_names.begin(), l_raw_names.end(), [](const QString &f_raw) {
                          bool l_is_area = false;
                          f_raw.split(":").first().toInt(&l_is_area);
                          return !l_is_area;
                      }),
                      l_raw_names.end());
    std::sort(l_raw_names.begin(), l_raw_names.end(), [](const QString &a, const QString &b) { return a.split(":")[0].toInt() < b.split(":")[0].toInt(); });
    if (l_raw_names.isEmpty()) {
        return QStringLiteral("The floor file defines no areas.");
    }

    // A floor of the same name is replaced; an unknown one is created.
    f_floor_id = -1;
    for (const Floor &l_floor : qAsConst(m_floors)) {
        if (l_floor.name.compare(f_name, Qt::CaseInsensitive) == 0) {
            f_floor_id = l_floor.id;
        }
    }

    if (f_floor_id >= 0) {
        f_mapping = compactAreas(m_floors[f_floor_id].area_ids);
        m_floors[f_floor_id].before_rules.clear();
        m_floors[f_floor_id].after_rules.clear();
        applyDefaultFloorRules(m_floors[f_floor_id]);
    }
    else {
        Floor l_floor;
        l_floor.id = m_floors.size();
        l_floor.name = f_name;
        m_floors.append(l_floor);
        f_floor_id = l_floor.id;
        applyDefaultFloorRules(m_floors[f_floor_id]);
        f_mapping.clear();
    }

    // The file's floor rules layer on the defaults, owner "config".
    const auto l_apply_rules = [this](const QVector<config::RuleDeclaration> &f_declarations,
                                      QVector<BeforeRuleEntry> &f_before, QVector<AfterRuleEntry> &f_after) {
        for (const config::RuleDeclaration &l_declaration : f_declarations) {
            if (l_declaration.phase == RulePhase::Before) {
                if (auto l_function = m_rules->buildBefore(l_declaration.action, *m_services, l_declaration.args)) {
                    f_before.append({l_declaration.event, l_declaration.action, *l_function, QStringLiteral("config"), l_declaration.args});
                    continue;
                }
            }
            else if (auto l_function = m_rules->buildAfter(l_declaration.action, *m_services, l_declaration.args)) {
                f_after.append({l_declaration.event, l_declaration.action, *l_function, QStringLiteral("config"), l_declaration.args});
                continue;
            }
            qWarning() << "Skipping floor-file rule for event" << l_declaration.event << "-" << l_declaration.action
                       << "is not a registered action of that phase.";
        }
    };
    for (auto it = l_rule_config.floor_rules.constBegin(); it != l_rule_config.floor_rules.constEnd(); ++it) {
        l_apply_rules(it.value(), m_floors[f_floor_id].before_rules, m_floors[f_floor_id].after_rules);
    }

    for (const QString &l_raw : qAsConst(l_raw_names)) {
        QStringList l_parts = l_raw.split(":");
        const int l_local_index = l_parts.takeFirst().toInt();
        const QString l_area_name = l_parts.join(":");

        const int l_area_id = m_areas.size();
        m_area_names.append(l_area_name);
        Area *l_area = buildArea(l_area_id, l_area_name, f_floor_id, &l_file_settings, l_raw);
        l_apply_rules(l_rule_config.area_rules.value(l_local_index), l_area->beforeRules(), l_area->afterRules());
    }
    return std::nullopt;
}

void World::rebuild(const QString &f_rules_path)
{
    for (Area *l_area : qAsConst(m_areas)) {
        Q_EMIT areaAboutToBeRemoved(l_area);
        l_area->deleteLater();
    }
    m_areas.clear();
    m_area_names.clear();
    m_floors.clear();

    buildFromConfig(f_rules_path);
}

} // namespace akashi
