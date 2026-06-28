#include "medieval_speak_plugin.h"

#include "akashi/config_store.h"
#include "akashi/filesystem_service.h"
#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/permission_registry.h"
#include "core/text_filter_registry.h"

#include "world/area.h"
#include "core/server_context.h"

#include <QDebug>

QString MedievalSpeakPlugin::id() const { return QStringLiteral("akashi.medieval-speak"); }
akashi::ServiceVersion MedievalSpeakPlugin::pluginVersion() const { return {1, 0, 0}; }

bool MedievalSpeakPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_fs = services.resolve<akashi::FileSystemService>(QStringLiteral("akashi.filesystem"));
    auto l_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));
    auto l_filters = services.resolve<akashi::TextFilterRegistry>(QStringLiteral("akashi.textfilters"));
    auto l_commands = services.resolve<akashi::CommandRegistry>(QStringLiteral("akashi.commands"));

    if (!l_fs || !l_config || !l_filters || !l_commands) {
        qWarning() << "medieval-speak: required services not available";
        return false;
    }

    const QString l_config_name = QStringLiteral("plugins/") + id();
    const QString l_default_data = QStringLiteral("text/autorp.json");
    l_config->declarePlugin(id(), {
        akashi::ConfigEntry(QStringLiteral("data_file"), l_default_data,
                            QStringLiteral("Path to the word replacement data, relative to config root."))
    });

    QString l_data_file = l_config->get<QString>(l_config_name, QStringLiteral("data_file"));
    QString l_resolved = l_config->filePath(l_data_file);

    m_parser = std::make_unique<MedievalParser>(l_resolved);

    const QString l_owner = id();
    MedievalParser *l_parser = m_parser.get();

    l_filters->registerFilter(QStringLiteral("medieval"), 300,
        [l_parser](const QString &f_text) -> std::optional<QString> {
            return l_parser->degrootify(f_text);
        }, false, l_owner);

    l_commands->registerCommand(
        {QStringLiteral("medieval"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/medieval <id>"),
         QStringLiteral("Makes a client speak in medieval English.")},
        [](akashi::CommandContext &f_context) {
            if (auto l_target = f_context.resolveTarget()) {
                if (l_target->hasSanction(akashi::sanction::medieval))
                    f_context.reply("That player is already speaking Ye Olde English!");
                else {
                    f_context.reply("It is done, sire.");
                    l_target->reply("Forsooth! Thine speech will henceforth be Ye Olde!");
                }
                l_target->setSanction(akashi::sanction::medieval, true);
            }
        }, l_owner);

    l_commands->registerCommand(
        {QStringLiteral("unmedieval"), {}, {akashi::permission::mute}, 1,
         QStringLiteral("/unmedieval <id>"),
         QStringLiteral("Returns a client to plain speech.")},
        [](akashi::CommandContext &f_context) {
            if (auto l_target = f_context.resolveTarget()) {
                if (!l_target->hasSanction(akashi::sanction::medieval))
                    f_context.reply("That player is not shaken!");
                else {
                    f_context.reply("Un-medieval'd player.");
                    l_target->reply("Hark! Thine speech hast been returneth to normal.");
                }
                l_target->setSanction(akashi::sanction::medieval, false);
            }
        }, l_owner);

    l_commands->registerCommand(
        {QStringLiteral("medievalmode"), {QStringLiteral("medieval_mode")}, {akashi::permission::mute}, 0,
         QStringLiteral("/medievalmode"),
         QStringLiteral("Toggles medieval mode in the area.")},
        [](akashi::CommandContext &f_context) {
            akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
            l_area->toggleMedievalMode();
            QString l_state = l_area->isMedievalMode() ? "enabled." : "disabled.";
            f_context.replyToArea("Hear ye, hear ye! Medieval Mode is now " + l_state);
        }, l_owner);

    qInfo().noquote() << "medieval-speak: word data from" << l_resolved;
    return true;
}

void MedievalSpeakPlugin::shutdown(akashi::ServiceRegistry &services)
{
    auto l_filters = services.resolve<akashi::TextFilterRegistry>(QStringLiteral("akashi.textfilters"));
    if (l_filters)
        l_filters->unregisterAll(id());

    auto l_commands = services.resolve<akashi::CommandRegistry>(QStringLiteral("akashi.commands"));
    if (l_commands)
        l_commands->unregisterAll(id());

    m_parser.reset();
}
