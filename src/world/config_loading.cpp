#include "world/config_loading.h"

#include "akashi/logging_categories.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace akashi {
namespace config {

QStringList loadTextFile(const QString &f_path)
{
    QStringList l_list;
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return l_list;
    }
    while (!l_file.atEnd()) {
        l_list.append(l_file.readLine().trimmed());
    }
    l_file.close();
    return l_list;
}

MusicCatalog loadMusicList(const QString &f_path)
{
    MusicCatalog l_catalog;
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(akashiConfig) << "Unable to open musiclist" << f_path;
        return l_catalog;
    }

    QJsonParseError l_error;
    QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll(), &l_error);
    if (l_error.error != QJsonParseError::NoError) {
        qCWarning(akashiConfig) << "Unable to load musiclist. The following error was encountered:" << l_error.errorString();
        return l_catalog;
    }

    QJsonArray l_root = l_doc.array();
    for (int i = 0; i < l_root.size(); i++) {
        QJsonObject l_child = l_root.at(i).toObject();

        QString l_category = l_child["category"].toString();
        if (!l_category.isEmpty()) {
            l_catalog.songs.insert(l_category, {l_category, 0});
            l_catalog.ordered.append(l_category);
        }
        else {
            qCWarning(akashiConfig) << "Category name not set. This may cause the musiclist to be displayed incorrectly.";
        }

        QJsonArray l_songs = l_child["songs"].toArray();
        for (int j = 0; j < l_songs.size(); j++) {
            QJsonObject l_song = l_songs.at(j).toObject();
            QString l_name = l_song["name"].toString();
            QString l_real = l_song["realname"].toString();
            if (l_real.isEmpty()) {
                l_real = l_name;
            }
            int l_duration = l_song["length"].toVariant().toInt();
            l_catalog.songs.insert(l_name, {l_real, l_duration});
            l_catalog.ordered.append(l_name);
        }
    }
    l_file.close();
    return l_catalog;
}

// The old check_setting name table is gone: the setting vocabulary is the
// Area property surface now, and the trap names either remap to their new
// spelling or refuse with the replacement named. Returns false when the
// declaration must be skipped.
static bool remapDeprecatedSetting(RuleDeclaration &f_declaration)
{
    if (f_declaration.action != QStringLiteral("check_setting"))
        return true;
    const QString l_setting = f_declaration.args.value(QStringLiteral("setting")).toString();
    if (l_setting == QStringLiteral("music")) {
        f_declaration.args.insert(QStringLiteral("setting"), QStringLiteral("music_allowed"));
        qCInfo(akashiConfig) << "check_setting setting \"music\" is deprecated - remapped to \"music_allowed\".";
        return true;
    }
    if (l_setting == QStringLiteral("blankposting")) {
        f_declaration.action = QStringLiteral("check_blankposting");
        f_declaration.args.remove(QStringLiteral("setting"));
        qCInfo(akashiConfig) << "check_setting setting \"blankposting\" is deprecated - remapped to the check_blankposting action.";
        return true;
    }
    if (l_setting == QStringLiteral("iniswap")) {
        f_declaration.action = QStringLiteral("check_iniswap");
        f_declaration.args.remove(QStringLiteral("setting"));
        qCInfo(akashiConfig) << "check_setting setting \"iniswap\" is deprecated - remapped to the check_iniswap action.";
        return true;
    }
    if (l_setting == QStringLiteral("shouts")) {
        qCWarning(akashiConfig) << "check_setting no longer gates \"shouts\" - shouts are downgraded by the strip_shouts transform rule. The rule was skipped.";
        return false;
    }
    if (l_setting == QStringLiteral("shownames")) {
        qCWarning(akashiConfig) << "check_setting no longer gates \"shownames\" - use the check_showname action. The rule was skipped.";
        return false;
    }
    if (l_setting == QStringLiteral("wtce")) {
        qCWarning(akashiConfig) << "check_setting no longer gates \"wtce\" - use the check_wtce action. The rule was skipped.";
        return false;
    }
    if (l_setting == QStringLiteral("ic_messages")) {
        qCWarning(akashiConfig) << "check_setting no longer gates \"ic_messages\" - the IC floodguard is not a rule. The rule was skipped.";
        return false;
    }
    return true;
}

// Reads the "before"/"transform"/"after" buckets of one "rules" object. Any
// key on an action object besides "action" travels along as an argument.
static QVector<RuleDeclaration> parseRules(const QJsonObject &f_rules)
{
    QVector<RuleDeclaration> l_result;
    for (auto it = f_rules.begin(); it != f_rules.end(); ++it) {
        const QJsonObject l_buckets = it.value().toObject();
        const QVector<std::pair<QString, RulePhase>> l_phases = {
            {QStringLiteral("before"), RulePhase::Before},
            {QStringLiteral("transform"), RulePhase::Transform},
            {QStringLiteral("after"), RulePhase::After},
        };
        for (const auto &l_phase : l_phases) {
            const QJsonArray l_actions = l_buckets.value(l_phase.first).toArray();
            for (const QJsonValue &l_value : l_actions) {
                const QJsonObject l_object = l_value.toObject();
                RuleDeclaration l_declaration;
                l_declaration.event = it.key();
                l_declaration.phase = l_phase.second;
                l_declaration.action = l_object.value(QStringLiteral("action")).toString();
                if (l_declaration.action.isEmpty()) {
                    qCWarning(akashiConfig) << "A rule for event" << it.key() << "names no action and was skipped.";
                    continue;
                }
                for (auto arg = l_object.begin(); arg != l_object.end(); ++arg) {
                    if (arg.key() != QStringLiteral("action"))
                        l_declaration.args.insert(arg.key(), arg.value().toVariant());
                }
                if (!remapDeprecatedSetting(l_declaration)) {
                    continue;
                }
                l_result.append(l_declaration);
            }
        }
    }
    return l_result;
}

AreaRulesConfig loadAreaRules(const QString &f_path)
{
    AreaRulesConfig l_config;
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return l_config;
    }

    QJsonParseError l_error;
    const QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll(), &l_error);
    if (l_error.error != QJsonParseError::NoError || !l_doc.isObject()) {
        qCWarning(akashiConfig) << "Unable to load area rules. The following error was encountered:" << l_error.errorString();
        return l_config;
    }

    const QJsonObject l_root = l_doc.object();
    const QJsonObject l_floors = l_root.value(QStringLiteral("floors")).toObject();
    for (auto it = l_floors.begin(); it != l_floors.end(); ++it) {
        const auto l_rules = parseRules(it.value().toObject().value(QStringLiteral("rules")).toObject());
        if (!l_rules.isEmpty())
            l_config.floor_rules.insert(it.key(), l_rules);
    }

    for (auto it = l_root.begin(); it != l_root.end(); ++it) {
        if (!it.value().isObject())
            continue;
        const QStringList l_parts = it.key().split(QLatin1Char(':'));
        bool l_is_area = false;
        const int l_index = l_parts.first().toInt(&l_is_area);
        if (!l_is_area || l_parts.size() < 2)
            continue;
        const auto l_rules = parseRules(it.value().toObject().value(QStringLiteral("rules")).toObject());
        if (!l_rules.isEmpty())
            l_config.area_rules.insert(l_index, l_rules);
    }
    return l_config;
}

QStringList loadIpRangeBans(const QString &f_path)
{
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    // A malformed ban file must be called out, or it silently loads as "no bans".
    QJsonParseError l_error;
    QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll(), &l_error);
    if (l_error.error != QJsonParseError::NoError || !l_doc.isObject()) {
        qCWarning(akashiConfig) << "Unable to load IP range bans from" << f_path << "- no bans applied. Error:" << l_error.errorString();
        return {};
    }

    QJsonObject l_root = l_doc.object();
    QStringList l_bans;
    l_bans.append(l_root["ip_range"].toVariant().toStringList());
    l_bans.removeDuplicates();
    return l_bans;
}

QList<quint32> loadBannedAsns(const QString &f_path)
{
    QList<quint32> l_asns;
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return l_asns;
    }
    // A malformed ban file must be called out, or it silently loads as "no bans".
    QJsonParseError l_error;
    const QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll(), &l_error);
    if (l_error.error != QJsonParseError::NoError || !l_doc.isObject()) {
        qCWarning(akashiConfig) << "Unable to load banned ASNs from" << f_path << "- no bans applied. Error:" << l_error.errorString();
        return l_asns;
    }
    const QStringList l_texts = l_doc.object()["asn"].toVariant().toStringList();
    for (const QString &l_text : l_texts) {
        bool l_ok;
        const quint32 l_asn = l_text.toUInt(&l_ok);
        if (!l_ok) {
            qCWarning(akashiConfig) << "Ignoring banned ASN entry that is not a number:" << l_text;
            continue;
        }
        l_asns.append(l_asn);
    }
    return l_asns;
}

} // namespace config
} // namespace akashi
