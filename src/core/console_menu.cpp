#include "core/console_menu.h"

#include "core/console_input.h"

#include "akashi/service_registry.h"
#include "commands/authentication_commands.h"
#include "core/client_session.h"
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

void ConsoleMenu::unregisterAll(const QString &f_owner_id)
{
    m_action_source->m_actions.removeIf([&f_owner_id](const ActionEntry &e) {
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

void ConsoleMenu::activate(int f_index)
{
    if (f_index >= 0 && f_index < m_items.size()) {
        m_items[f_index].activate();
    }
}

void ConsoleMenu::goBack()
{
    if (m_back) {
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
        {QStringLiteral("tasks (%1)").arg(m_action_source->m_actions.size()), [this] { openTasks(); }},
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

void ConsoleMenu::openTasks()
{
    const QList<ActionEntry> &l_actions = m_action_source->m_actions;
    m_title = QStringLiteral("tasks (%1)").arg(l_actions.size());
    m_hint = m_interactive ? QStringLiteral("enter runs a task, esc goes back")
                           : QStringLiteral("enter a task number; 0 goes back");
    m_back = [this] { openMain(); };
    m_selected = 0;
    m_text_entry = false;
    m_items.clear();
    for (const ActionEntry &l_entry : l_actions) {
        // Resolved by title when run, so a task whose plugin has since
        // unloaded degrades to a notice instead of a dangling call.
        const QString l_title = l_entry.title;
        m_items.append({l_title, [this, l_title] {
                            for (const ActionEntry &l_live : std::as_const(m_action_source->m_actions)) {
                                if (l_live.title == l_title) {
                                    m_rendered_lines = 0;
                                    l_live.action();
                                    render();
                                    return;
                                }
                            }
                            printOut(QStringLiteral("That task is gone; its plugin was unloaded."));
                            openTasks();
                        }});
    }
    if (m_items.isEmpty()) {
        m_items.append({QStringLiteral("no plugin registered a task - back"), [this] { openMain(); }});
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

void ConsoleMenu::openAuthentication()
{
    const bool l_simple = m_server->authType() == AuthType::SIMPLE;
    m_title = QStringLiteral("authentication | mode: %1")
                  .arg(l_simple ? QStringLiteral("simple (one shared modpass)")
                                : QStringLiteral("advanced (user accounts)"));
    m_hint = m_interactive ? QStringLiteral("enter selects, esc goes back")
                           : QStringLiteral("enter a number; 0 goes back");
    m_back = [this] { openMain(); };
    m_selected = 0;
    m_text_entry = false;
    m_items.clear();
    if (l_simple) {
        m_items.append({QStringLiteral("switch to advanced (set the root password)"), [this] { openRootPasswordPrompt(); }});
    }
    else {
        m_items.append({QStringLiteral("reset the root password"), [this] { openRootPasswordPrompt(); }});
        m_items.append({QStringLiteral("switch to simple (single modpass, keeps the user database)"), [this] {
                            m_server->setAuthType(AuthType::SIMPLE);
                            QString l_note = QStringLiteral("Switched to simple authentication. Moderators log in with the modpass.");
                            if (m_server->serverSettings()->modpass().isEmpty()) {
                                l_note += QStringLiteral("\nWarning: no modpass is set, so nobody can log in until one is configured.");
                            }
                            printOut(l_note);
                            openAuthentication();
                        }});
    }
    m_items.append({QStringLiteral("back"), [this] { openMain(); }});
    m_rendered_lines = 0;
    render();
}

void ConsoleMenu::openRootPasswordPrompt()
{
    m_back = [this] { openAuthentication(); };
    openTextEntry(QStringLiteral("root password"), QStringLiteral("new password"), true, [this](const QString &f_password) {
        applyRootPassword(f_password);
    });
}

void ConsoleMenu::applyRootPassword(const QString &f_password)
{
    if (!commands::passwordMeetsRequirements(m_server->serverSettings(), QStringLiteral("root"), f_password)) {
        printOut(QStringLiteral("That password does not meet the server's password requirements."));
        openAuthentication();
        return;
    }

    DBManager *l_db = m_server->databaseManager();
    bool l_stored = l_db->createUser(QStringLiteral("root"), CryptoHelper::randbytes(16), f_password, ACLRolesHandler::SUPER_ID);
    if (!l_stored) {
        // The root account already exists; refresh its credentials instead.
        l_stored = l_db->updatePassword(QStringLiteral("root"), f_password) && l_db->updateACL(QStringLiteral("root"), ACLRolesHandler::SUPER_ID);
    }
    if (!l_stored) {
        printOut(QStringLiteral("Unable to store the root account; see the log."));
        openAuthentication();
        return;
    }

    const bool l_switched = m_server->authType() != AuthType::ADVANCED;
    m_server->setAuthType(AuthType::ADVANCED);
    printOut(l_switched ? QStringLiteral("Switched to advanced authentication. Log in with /login root [password].")
                        : QStringLiteral("Root password updated."));
    openAuthentication();
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
