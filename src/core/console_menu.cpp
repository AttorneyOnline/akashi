#include "core/console_menu.h"

#include "akashi/config_store.h"
#include "akashi/service_registry.h"
#include "commands/authentication_commands.h"
#include "core/client_session.h"
#include "core/console_input.h"
#include "core/crypto_helper.h"
#include "core/db_manager.h"
#include "core/permission_registry.h"
#include "core/plugin_manager.h"
#include "core/server_context.h"
#include "core/server_settings.h"
#include "proto/packet.h"
#include "softwareinformation.h"

#include <QCoreApplication>

#include <algorithm>
#include <cstdio>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

QString pluginStateName(akashi::PluginInfo::State f_state)
{
    switch (f_state) {
    case akashi::PluginInfo::State::Discovered:
        return QStringLiteral("stopped");
    case akashi::PluginInfo::State::Loaded:
        return QStringLiteral("loaded");
    case akashi::PluginInfo::State::Initialized:
        return QStringLiteral("initialized");
    case akashi::PluginInfo::State::Started:
        return QStringLiteral("running");
    case akashi::PluginInfo::State::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

bool enableVtOnStdout()
{
#ifdef Q_OS_WIN
    if (!_isatty(_fileno(stdout))) {
        return false;
    }
    HANDLE l_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD l_mode = 0;
    if (l_handle == INVALID_HANDLE_VALUE || !GetConsoleMode(l_handle, &l_mode)) {
        return false;
    }
    return SetConsoleMode(l_handle, l_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

} // namespace

namespace akashi {

ConsoleMenu *ConsoleMenu::s_active_session = nullptr;

ConsoleMenu *ConsoleMenu::activeSession()
{
    return s_active_session;
}

ConsoleMenu::ConsoleMenu(ServerContext *f_server, QObject *parent) :
    QObject(parent),
    m_server(f_server),
    m_action_source(this)
{}

QString ConsoleMenu::serviceId() const
{
    return QStringLiteral("akashi.console");
}

ServiceVersion ConsoleMenu::serviceVersion() const
{
    return {1, 0, 0};
}

void ConsoleMenu::setInteractive(bool f_interactive)
{
    m_interactive = f_interactive;
    m_vt = f_interactive && enableVtOnStdout();
}

void ConsoleMenu::setInteractive(bool f_interactive, bool f_vt)
{
    m_interactive = f_interactive;
    m_vt = f_interactive && f_vt;
}

void ConsoleMenu::setSink(std::function<void(const QByteArray &)> f_sink)
{
    m_sink = std::move(f_sink);
}

ConsoleMenu *ConsoleMenu::createSession(QObject *f_parent)
{
    auto *l_session = new ConsoleMenu(m_server, f_parent);
    l_session->m_action_source = m_action_source;
    return l_session;
}

void ConsoleMenu::show()
{
    openMain();
}

bool ConsoleMenu::registerAction(const QString &f_title, std::function<void()> f_action,
                                 const QString &f_owner_id)
{
    if (f_title.isEmpty() || !f_action) {
        return false;
    }
    QList<ActionEntry> &l_actions = m_action_source->m_actions;
    for (const ActionEntry &l_entry : std::as_const(l_actions)) {
        if (l_entry.title == f_title) {
            return false;
        }
    }
    l_actions.append({f_title, std::move(f_action), f_owner_id});
    return true;
}

bool ConsoleMenu::registerScheduledAction(const QString &f_title,
                                          std::function<std::optional<QDateTime>()> f_next_run,
                                          std::function<void()> f_action,
                                          const QString &f_owner_id)
{
    if (f_title.isEmpty() || !f_next_run || !f_action) {
        return false;
    }
    QList<ScheduledEntry> &l_scheduled = m_action_source->m_scheduled;
    for (const ScheduledEntry &l_entry : std::as_const(l_scheduled)) {
        if (l_entry.title == f_title) {
            return false;
        }
    }
    l_scheduled.append({f_title, std::move(f_next_run), std::move(f_action), f_owner_id});
    return true;
}

void ConsoleMenu::unregisterAll(const QString &f_owner_id)
{
    m_action_source->m_actions.removeIf([&f_owner_id](const ActionEntry &e) {
        return e.owner_id == f_owner_id;
    });
    m_action_source->m_scheduled.removeIf([&f_owner_id](const ScheduledEntry &e) {
        return e.owner_id == f_owner_id;
    });
}

// Prints action output between menus; the next render starts fresh below
// it instead of repainting over it.
void ConsoleMenu::printOut(const QString &f_text)
{
    writeOut(f_text.toUtf8() + '\n');
    m_rendered_lines = 0;
}

void ConsoleMenu::writeOut(const QByteArray &f_bytes)
{
    if (m_sink) {
        m_sink(f_bytes);
        return;
    }
    std::fwrite(f_bytes.constData(), 1, f_bytes.size(), stdout);
    std::fflush(stdout);
}

void ConsoleMenu::render()
{
    QString l_out;
    // Repainting in place keeps the menu still while the highlight moves.
    if (m_vt && m_rendered_lines > 0) {
        l_out += QStringLiteral("\x1b[%1A\x1b[0J").arg(m_rendered_lines);
    }

    int l_lines = 0;
    const auto l_add = [&l_out, &l_lines](const QString &f_line) {
        l_out += f_line + QLatin1Char('\n');
        l_lines++;
    };

    l_add(QStringLiteral("--- %1 ---").arg(m_title));
    if (m_text_entry) {
        l_add(QStringLiteral("%1: %2_").arg(m_text_prompt, m_text_masked ? QString(m_text_input.size(), QLatin1Char('*')) : m_text_input));
    }
    else {
        for (int i = 0; i < m_items.size(); i++) {
            const bool l_current = m_interactive && i == m_selected;
            QString l_line = QStringLiteral(" %1 %2  %3")
                                 .arg(l_current ? QStringLiteral(">") : QStringLiteral(" "))
                                 .arg(i + 1)
                                 .arg(m_items[i].label);
            if (l_current && m_vt) {
                l_line = QStringLiteral("\x1b[36m") + l_line + QStringLiteral("\x1b[0m");
            }
            l_add(l_line);
        }
    }
    l_add(m_hint);

    writeOut(l_out.toUtf8());
    m_rendered_lines = l_lines;
}

// A clean screen for whatever an action prints or opens; the old content
// stays in the terminal's scrollback. Non-VT terminals keep appending.
void ConsoleMenu::clearScreen()
{
    if (!m_vt) {
        return;
    }
    writeOut(QByteArrayLiteral("\x1b[2J\x1b[H"));
    m_rendered_lines = 0;
}

void ConsoleMenu::activate(int f_index)
{
    if (f_index >= 0 && f_index < m_items.size()) {
        clearScreen();
        m_items[f_index].activate();
    }
}

void ConsoleMenu::goBack()
{
    if (m_back) {
        clearScreen();
        m_back();
    }
    else {
        // Backing out at the top starts a fresh paint below the log lines.
        m_rendered_lines = 0;
        render();
    }
}

void ConsoleMenu::handleKey(int f_key, QChar f_character)
{
    switch (f_key) {
    case ConsoleInput::KeyUp:
        if (!m_text_entry && !m_items.isEmpty()) {
            m_selected = (m_selected + m_items.size() - 1) % m_items.size();
            render();
        }
        break;
    case ConsoleInput::KeyDown:
        if (!m_text_entry && !m_items.isEmpty()) {
            m_selected = (m_selected + 1) % m_items.size();
            render();
        }
        break;
    case ConsoleInput::KeyEnter:
        if (m_text_entry) {
            submitTextEntry(m_text_input.trimmed());
        }
        else {
            activate(m_selected);
        }
        break;
    case ConsoleInput::KeyBack:
        if (m_text_entry) {
            submitTextEntry(QString());
        }
        else {
            goBack();
        }
        break;
    case ConsoleInput::KeyBackspace:
        if (m_text_entry && !m_text_input.isEmpty()) {
            m_text_input.chop(1);
            render();
        }
        break;
    case ConsoleInput::KeyCharacter:
        if (m_text_entry) {
            m_text_input += f_character;
            render();
        }
        else if (f_character.isDigit()) {
            const int l_number = f_character.digitValue();
            if (l_number == 0) {
                goBack();
            }
            else if (l_number <= m_items.size()) {
                m_selected = l_number - 1;
                activate(m_selected);
            }
        }
        break;
    }
}

void ConsoleMenu::handleLine(const QString &f_line)
{
    if (m_text_entry) {
        submitTextEntry(f_line.trimmed());
        return;
    }

    if (f_line.isEmpty()) {
        m_rendered_lines = 0;
        render();
        return;
    }

    bool l_ok = false;
    const int l_number = f_line.toInt(&l_ok);
    if (!l_ok) {
        render();
        return;
    }
    if (l_number == 0) {
        goBack();
    }
    else if (l_number <= m_items.size()) {
        m_selected = l_number - 1;
        activate(m_selected);
    }
    else {
        render();
    }
}

void ConsoleMenu::openMain()
{
    m_title = QStringLiteral("akashi console");
    m_hint = m_interactive ? QStringLiteral("arrows move, enter selects, esc redraws")
                           : QStringLiteral("enter a number; a blank line redraws");
    m_back = nullptr;
    m_selected = 0;
    m_text_entry = false;
    m_items = {
        {QStringLiteral("status"), [this] { printStatus(); render(); }},
        {QStringLiteral("players"), [this] { openPlayers(); }},
        {QStringLiteral("plugins"), [this] { openPlugins(); }},
        {QStringLiteral("tasks (%1)").arg(m_action_source->m_actions.size() + m_action_source->m_scheduled.size()), [this] { openTasks(); }},
        {QStringLiteral("broadcast"), [this] { openBroadcast(); }},
        {QStringLiteral("authentication"), [this] { openAuthentication(); }},
        {QStringLiteral("reload configuration"), [this] {
             m_server->reloadSettings();
             printOut(QStringLiteral("Configuration reloaded."));
             render();
         }},
        {QStringLiteral("shut down"), [this] { openConfirmShutdown(); }},
    };
    m_rendered_lines = 0;
    render();
}

void ConsoleMenu::printStatus()
{
    int l_running = 0;
    int l_total = 0;
    if (auto l_plugins = m_server->services()->resolve<PluginManager>(QStringLiteral("akashi.plugins"))) {
        const auto l_list = l_plugins->plugins();
        l_total = l_list.size();
        for (const PluginInfo &l_info : l_list) {
            if (l_info.state == PluginInfo::State::Started) {
                l_running++;
            }
        }
    }
    printOut(QStringLiteral("akashi %1 | players %2/%3 | areas %4 | plugins %5 of %6 running")
                 .arg(software::fullVersion())
                 .arg(m_server->playerCount())
                 .arg(m_server->serverSettings()->max_players())
                 .arg(m_server->areaNames().size())
                 .arg(l_running)
                 .arg(l_total));
}

void ConsoleMenu::openPlayers()
{
    const QVector<ClientSession *> l_clients = m_server->clients();
    m_title = QStringLiteral("players (%1)").arg(l_clients.size());
    m_hint = m_interactive ? QStringLiteral("enter opens a player, esc goes back")
                           : QStringLiteral("enter a row number; 0 goes back");
    m_back = [this] { openMain(); };
    m_selected = 0;
    m_text_entry = false;
    m_items.clear();
    for (ClientSession *l_client : l_clients) {
        const int l_id = l_client->clientId();
        m_items.append({QStringLiteral("[%1] %2 %3 in %4 (%5)")
                            .arg(l_id)
                            .arg(l_client->ipid(),
                                 l_client->character().isEmpty() ? QStringLiteral("spectator") : l_client->character(),
                                 m_server->areaName(l_client->areaId()),
                                 l_client->name().isEmpty() ? QStringLiteral("unnamed") : l_client->name()),
                        [this, l_id] { openPlayerActions(l_id); }});
    }
    if (m_items.isEmpty()) {
        m_items.append({QStringLiteral("nobody is connected - back"), [this] { openMain(); }});
    }
    m_rendered_lines = 0;
    render();
}

void ConsoleMenu::openPlayerActions(int f_client_id)
{
    ClientSession *l_client = m_server->clientById(f_client_id);
    if (!l_client) {
        printOut(QStringLiteral("That player is gone."));
        openPlayers();
        return;
    }
    m_title = QStringLiteral("player [%1] %2 | ipid %3 | %4 in %5")
                  .arg(f_client_id)
                  .arg(l_client->name().isEmpty() ? QStringLiteral("unnamed") : l_client->name(),
                       l_client->ipid(),
                       l_client->character().isEmpty() ? QStringLiteral("spectator") : l_client->character(),
                       m_server->areaName(l_client->areaId()));
    m_hint = m_interactive ? QStringLiteral("enter selects, esc goes back")
                           : QStringLiteral("enter a number; 0 goes back");
    m_back = [this] { openPlayers(); };
    m_selected = 0;
    m_text_entry = false;
    m_items = {
        {QStringLiteral("kick"), [this, f_client_id] {
             ClientSession *l_target = m_server->clientById(f_client_id);
             if (l_target) {
                 l_target->sendPacket(QStringLiteral("KK"), {QStringLiteral("Kicked from the server console.")});
                 l_target->closeSocket();
                 printOut(QStringLiteral("Kicked."));
             }
             else {
                 printOut(QStringLiteral("That player is gone."));
             }
             openPlayers();
         }},
        {QStringLiteral("back"), [this] { openPlayers(); }},
    };
    m_rendered_lines = 0;
    render();
}

void ConsoleMenu::openPlugins()
{
    auto l_manager = m_server->services()->resolve<PluginManager>(QStringLiteral("akashi.plugins"));
    QList<PluginInfo> l_plugins = l_manager ? l_manager->plugins() : QList<PluginInfo>();
    std::sort(l_plugins.begin(), l_plugins.end(), [](const PluginInfo &a, const PluginInfo &b) {
        return a.id < b.id;
    });

    m_title = QStringLiteral("plugins (%1)").arg(l_plugins.size());
    m_hint = m_interactive ? QStringLiteral("enter opens a plugin, esc goes back")
                           : QStringLiteral("enter a row number; 0 goes back");
    m_back = [this] { openMain(); };
    m_selected = 0;
    m_text_entry = false;
    m_items.clear();
    for (const PluginInfo &l_info : std::as_const(l_plugins)) {
        QString l_label = QStringLiteral("%1 v%2 [%3]").arg(l_info.id, l_info.version.toString(), pluginStateName(l_info.state));
        if (!l_info.runtime.isEmpty()) {
            l_label += QStringLiteral(" (%1)").arg(l_info.runtime);
        }
        if (l_info.state == PluginInfo::State::Started) {
            l_label += QStringLiteral(" · %1 ms").arg(l_info.boot_ms);
        }
        const QString l_id = l_info.id;
        m_items.append({l_label, [this, l_id] { openPluginActions(l_id); }});
    }
    if (m_items.isEmpty()) {
        m_items.append({QStringLiteral("no plugins discovered - back"), [this] { openMain(); }});
    }
    m_rendered_lines = 0;
    render();
}

void ConsoleMenu::openPluginActions(const QString &f_plugin_id)
{
    auto l_manager = m_server->services()->resolve<PluginManager>(QStringLiteral("akashi.plugins"));
    const auto l_info = l_manager ? l_manager->pluginInfo(f_plugin_id) : std::nullopt;
    if (!l_info) {
        openPlugins();
        return;
    }
    m_title = QStringLiteral("plugin %1 v%2 [%3]").arg(l_info->id, l_info->version.toString(), pluginStateName(l_info->state));
    m_hint = m_interactive ? QStringLiteral("enter selects, esc goes back")
                           : QStringLiteral("enter a number; 0 goes back");
    m_back = [this] { openPlugins(); };
    m_selected = 0;
    m_text_entry = false;
    const QString l_id = f_plugin_id;
    m_items = {
        {QStringLiteral("load"), [this, l_id, l_manager] {
             printOut(l_manager->loadPlugin(l_id) ? QStringLiteral("Loaded.") : QStringLiteral("Unable to load it; see the log."));
             openPlugins();
         }},
        {QStringLiteral("unload"), [this, l_id, l_manager] {
             printOut(l_manager->unloadPlugin(l_id) ? QStringLiteral("Unloaded.")
                                                    : QStringLiteral("Unable to unload it; other plugins may depend on it."));
             openPlugins();
         }},
        {QStringLiteral("unload with dependents"), [this, l_id, l_manager] {
             printOut(l_manager->unloadPlugin(l_id, true) ? QStringLiteral("Unloaded with its dependents.")
                                                          : QStringLiteral("Unable to unload it."));
             openPlugins();
         }},
        {QStringLiteral("reload"), [this, l_id, l_manager] {
             printOut(l_manager->reloadPlugin(l_id) ? QStringLiteral("Reloaded.") : QStringLiteral("Unable to reload it; see the log."));
             openPlugins();
         }},
        {QStringLiteral("back"), [this] { openPlugins(); }},
    };
    m_rendered_lines = 0;
    render();
}

// Turns a next-run time into the label suffix of a scheduled task.
QString ConsoleMenu::describeNextRun(const std::optional<QDateTime> &f_when)
{
    if (!f_when.has_value() || !f_when->isValid()) {
        return QStringLiteral("not scheduled");
    }
    const qint64 l_seconds = QDateTime::currentDateTime().secsTo(*f_when);
    if (l_seconds < 60) {
        return QStringLiteral("next run under a minute");
    }
    if (l_seconds < 60 * 60) {
        return QStringLiteral("next run in %1m").arg(l_seconds / 60);
    }
    if (l_seconds < 24 * 60 * 60) {
        return QStringLiteral("next run in %1h %2m").arg(l_seconds / 3600).arg(l_seconds % 3600 / 60);
    }
    return QStringLiteral("next run in %1d %2h").arg(l_seconds / 86400).arg(l_seconds % 86400 / 3600);
}

void ConsoleMenu::openTasks()
{
    const QList<ActionEntry> &l_actions = m_action_source->m_actions;
    const QList<ScheduledEntry> &l_scheduled = m_action_source->m_scheduled;
    m_title = QStringLiteral("tasks (%1)").arg(l_actions.size() + l_scheduled.size());
    m_hint = m_interactive ? QStringLiteral("enter runs a task, esc goes back")
                           : QStringLiteral("enter a task number; 0 goes back");
    m_back = [this] { openMain(); };
    m_selected = 0;
    m_text_entry = false;
    m_items.clear();
    for (const ScheduledEntry &l_entry : l_scheduled) {
        // Resolved by title when run, like the plain tasks below; the view
        // reopens afterwards so the next-run column stays current.
        const QString l_title = l_entry.title;
        m_items.append({QStringLiteral("%1 - %2").arg(l_title, describeNextRun(l_entry.next_run())),
                        [this, l_title] {
                            for (const ScheduledEntry &l_live : std::as_const(m_action_source->m_scheduled)) {
                                if (l_live.title == l_title) {
                                    m_rendered_lines = 0;
                                    s_active_session = this;
                                    l_live.action();
                                    s_active_session = nullptr;
                                    openTasks();
                                    return;
                                }
                            }
                            printOut(QStringLiteral("That task is gone; its owner was unloaded."));
                            openTasks();
                        }});
    }
    for (const ActionEntry &l_entry : l_actions) {
        // Resolved by title when run, so a task whose plugin has since
        // unloaded degrades to a notice instead of a dangling call.
        const QString l_title = l_entry.title;
        m_items.append({l_title, [this, l_title] {
                            for (const ActionEntry &l_live : std::as_const(m_action_source->m_actions)) {
                                if (l_live.title == l_title) {
                                    m_rendered_lines = 0;
                                    // The task's output belongs to whoever ran it.
                                    s_active_session = this;
                                    l_live.action();
                                    s_active_session = nullptr;
                                    render();
                                    return;
                                }
                            }
                            printOut(QStringLiteral("That task is gone; its plugin was unloaded."));
                            openTasks();
                        }});
    }
    if (m_items.isEmpty()) {
        m_items.append({QStringLiteral("no tasks registered - back"), [this] { openMain(); }});
    }
    m_rendered_lines = 0;
    render();
}

void ConsoleMenu::openBroadcast()
{
    m_back = [this] { openMain(); };
    openTextEntry(QStringLiteral("broadcast"), QStringLiteral("message"), false, [this](const QString &f_message) {
        m_server->broadcast(Packet(QStringLiteral("CT"), {m_server->serverNickname(), f_message, QStringLiteral("1")}));
        printOut(QStringLiteral("Broadcast sent."));
        openMain();
    });
}

// The user-management surface. The active system is read-only for the
// server's whole life, so the view manages the accounts of the running
// mode instead of offering to switch it.
void ConsoleMenu::openAuthentication()
{
    const QString l_system_id = m_server->activeAuthSystemId();
    QString l_mode = l_system_id;
    if (l_system_id == QStringLiteral("password")) {
        l_mode = QStringLiteral("simple (one shared modpass)");
    }
    else if (l_system_id == QStringLiteral("username")) {
        l_mode = QStringLiteral("advanced (user accounts)");
    }
    m_title = QStringLiteral("authentication | mode: %1").arg(l_mode);
    m_hint = m_interactive ? QStringLiteral("enter selects, esc goes back")
                           : QStringLiteral("enter a number; 0 goes back");
    m_back = [this] { openMain(); };
    m_selected = 0;
    m_text_entry = false;
    m_items.clear();
    if (l_system_id == QStringLiteral("username")) {
        m_items.append({QStringLiteral("list users"), [this] { openListUsers(); }});
        m_items.append({QStringLiteral("add a user"), [this] { openAddUserPrompt(); }});
        m_items.append({QStringLiteral("remove a user"), [this] { openRemoveUserPrompt(); }});
        m_items.append({QStringLiteral("assign a role"), [this] { openAssignRolePrompt(); }});
        m_items.append({QStringLiteral("change a password"), [this] { openChangePasswordPrompt(); }});
    }
    else if (l_system_id == QStringLiteral("password")) {
        m_items.append({QStringLiteral("change the modpass"), [this] { openModpassPrompt(); }});
    }
    // A plugin-provided system manages its own accounts; only back remains.
    m_items.append({QStringLiteral("back"), [this] { openMain(); }});
    m_rendered_lines = 0;
    render();
}

// Repackages /listusers with the role column; DBManager is the same
// source of truth the command reads.
void ConsoleMenu::openListUsers()
{
    DBManager *l_db = m_server->databaseManager();
    const QStringList l_users = l_db->users();
    if (l_users.isEmpty()) {
        printOut(QStringLiteral("No users are in the database."));
    }
    for (const QString &l_user : l_users) {
        printOut(QStringLiteral("%1 - %2").arg(l_user, l_db->acl(l_user)));
    }
    openAuthentication();
}

// Mirrors /adduser: a fresh user starts with the NONE role, and the
// password must meet the owner-configured requirements.
void ConsoleMenu::openAddUserPrompt()
{
    m_back = [this] { openAuthentication(); };
    openTextEntry(QStringLiteral("add a user"), QStringLiteral("username"), false, [this](const QString &f_username) {
        m_back = [this] { openAuthentication(); };
        openTextEntry(QStringLiteral("add a user"), QStringLiteral("password"), true, [this, f_username](const QString &f_password) {
            if (!commands::passwordMeetsRequirements(m_server->serverSettings(), f_username, f_password)) {
                printOut(QStringLiteral("That password does not meet the server's password requirements."));
                openAuthentication();
                return;
            }
            printOut(m_server->databaseManager()->createUser(f_username, CryptoHelper::randbytes(16), f_password, ACLRolesHandler::NONE_ID)
                         ? QStringLiteral("Created user %1. Assign a role to give them permissions.").arg(f_username)
                         : QStringLiteral("Unable to create user %1; does a user with that name already exist?").arg(f_username));
            openAuthentication();
        });
    });
}

void ConsoleMenu::openRemoveUserPrompt()
{
    m_back = [this] { openAuthentication(); };
    openTextEntry(QStringLiteral("remove a user"), QStringLiteral("username"), false, [this](const QString &f_username) {
        if (f_username == QStringLiteral("root")) {
            printOut(QStringLiteral("The root account cannot be removed."));
            openAuthentication();
            return;
        }
        printOut(m_server->databaseManager()->deleteUser(f_username)
                     ? QStringLiteral("Removed user %1.").arg(f_username)
                     : QStringLiteral("Unable to remove user %1; does it exist?").arg(f_username));
        openAuthentication();
    });
}

// Mirrors /setperms' guards. Its SUPER escalation rule - only a super may
// grant SUPER - passes trivially here, because the console operator IS
// super; root's role stays untouchable like everywhere else.
void ConsoleMenu::openAssignRolePrompt()
{
    m_back = [this] { openAuthentication(); };
    openTextEntry(QStringLiteral("assign a role"), QStringLiteral("username"), false, [this](const QString &f_username) {
        if (f_username == QStringLiteral("root")) {
            printOut(QStringLiteral("You can't change root's role!"));
            openAuthentication();
            return;
        }
        m_back = [this] { openAuthentication(); };
        openTextEntry(QStringLiteral("assign a role"), QStringLiteral("role id"), false, [this, f_username](const QString &f_role_id) {
            if (!m_server->aclRolesHandler()->roleExists(f_role_id)) {
                printOut(QStringLiteral("That role doesn't exist!"));
                openAuthentication();
                return;
            }
            printOut(m_server->databaseManager()->updateACL(f_username, f_role_id)
                         ? QStringLiteral("Assigned role %1 to user %2.").arg(f_role_id, f_username)
                         : QStringLiteral("%1 wasn't found!").arg(f_username));
            openAuthentication();
        });
    });
}

// Mirrors /changepass for any user, root included - this is the recovery
// path for a lost root password.
void ConsoleMenu::openChangePasswordPrompt()
{
    m_back = [this] { openAuthentication(); };
    openTextEntry(QStringLiteral("change a password"), QStringLiteral("username"), false, [this](const QString &f_username) {
        m_back = [this] { openAuthentication(); };
        openTextEntry(QStringLiteral("change a password"), QStringLiteral("new password"), true, [this, f_username](const QString &f_password) {
            if (!commands::passwordMeetsRequirements(m_server->serverSettings(), f_username, f_password)) {
                printOut(QStringLiteral("That password does not meet the server's password requirements."));
                openAuthentication();
                return;
            }
            printOut(m_server->databaseManager()->updatePassword(f_username, f_password)
                         ? QStringLiteral("Changed the password of %1.").arg(f_username)
                         : QStringLiteral("There was an error changing the password; does %1 exist?").arg(f_username));
            openAuthentication();
        });
    });
}

// The one simple-mode task. The write goes through the ConfigStore, so
// the value persists in config.json and the live modpass() read - the
// store's cached declared value - is current immediately.
void ConsoleMenu::openModpassPrompt()
{
    m_back = [this] { openAuthentication(); };
    openTextEntry(QStringLiteral("change the modpass"), QStringLiteral("new modpass"), true, [this](const QString &f_modpass) {
        m_server->serverSettings()->modpass.set(f_modpass);
        m_server->configStore()->settings(QStringLiteral("config"))->sync();
        printOut(QStringLiteral("Modpass changed; it applies to the next login."));
        openAuthentication();
    });
}

void ConsoleMenu::openTextEntry(const QString &f_title, const QString &f_prompt, bool f_masked,
                                std::function<void(const QString &)> f_submit)
{
    m_title = f_title;
    m_hint = m_interactive ? QStringLiteral("enter submits, esc cancels")
                           : QStringLiteral("press enter to submit; an empty line cancels");
    m_text_entry = true;
    m_text_masked = f_masked;
    m_text_prompt = f_prompt;
    m_text_input.clear();
    m_text_submit = std::move(f_submit);
    m_rendered_lines = 0;
    render();
}

// Every text-entry exit funnels through here: an empty submission cancels
// back to the view that opened the prompt.
void ConsoleMenu::submitTextEntry(const QString &f_text)
{
    m_text_entry = false;
    m_text_masked = false;
    m_text_input.clear();
    const auto l_submit = std::move(m_text_submit);
    m_text_submit = nullptr;
    clearScreen();
    if (f_text.isEmpty()) {
        printOut(QStringLiteral("Cancelled."));
        goBack();
        return;
    }
    if (l_submit) {
        l_submit(f_text);
    }
}

void ConsoleMenu::openConfirmShutdown()
{
    m_title = QStringLiteral("shut down");
    m_hint = m_interactive ? QStringLiteral("enter selects, esc goes back")
                           : QStringLiteral("enter a number; 0 goes back");
    m_back = [this] { openMain(); };
    m_selected = 1;
    m_text_entry = false;
    m_items = {
        {QStringLiteral("shut the server down now"), [this] {
             printOut(QStringLiteral("Shutting down."));
             // Deferred: quit() emits aboutToQuit synchronously, and the
             // teardown may own the very session this call stands on.
             QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
         }},
        {QStringLiteral("keep running"), [this] { openMain(); }},
    };
    m_rendered_lines = 0;
    render();
}

} // namespace akashi
