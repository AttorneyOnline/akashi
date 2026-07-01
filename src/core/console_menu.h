#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class ServerContext;

namespace akashi {

// The interactive menu on the server's terminal: navigable views for the
// day-to-day operator tasks, so nothing needs a memorized command line or
// a logged-in game client. On a live terminal the arrow keys move a
// highlight, enter selects and escape backs out; numbers work everywhere,
// including line-fed input. Plugins add their own tasks through
// registerAction, and a plugin's tasks leave with it.
class AKASHI_CORE_EXPORT ConsoleMenu : public QObject, public IService
{
    Q_OBJECT

  public:
    explicit ConsoleMenu(ServerContext *f_server, QObject *parent = nullptr);

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // Switches on arrow-key navigation with in-place repainting; whether
    // that repainting works is detected on the server's own terminal.
    void setInteractive(bool f_interactive);

    // The remote form: the transport decides both flags itself.
    void setInteractive(bool f_interactive, bool f_vt);

    // Routes all menu output through the given writer instead of the
    // server's terminal. Attached sessions write to their socket with this.
    void setSink(std::function<void(const QByteArray &)> f_sink);

    // A menu of its own for one attached operator: same views, same plugin
    // tasks, but its own navigation state and output sink. The caller owns
    // the returned menu through the parent.
    ConsoleMenu *createSession(QObject *f_parent);

    // Prints the main menu; call once the server runs.
    void show();

    // Adds a task to the menu's tasks view. The action prints its own
    // output. Returns false when the title is empty or already taken.
    bool registerAction(const QString &f_title, std::function<void()> f_action,
                        const QString &f_owner_id = {});

    void unregisterAll(const QString &f_owner_id);

  public Q_SLOTS:
    void handleKey(int f_key, QChar f_character);
    void handleLine(const QString &f_line);

  private:
    struct Item
    {
        QString label;
        std::function<void()> activate;
    };

    struct ActionEntry
    {
        QString title;
        std::function<void()> action;
        QString owner_id;
    };

    void render();
    void printOut(const QString &f_text);
    void writeOut(const QByteArray &f_bytes);
    void activate(int f_index);
    void goBack();

    void openMain();
    void openPlayers();
    void openPlayerActions(int f_client_id);
    void openPlugins();
    void openPluginActions(const QString &f_plugin_id);
    void openTasks();
    void openBroadcast();
    void openAuthentication();
    void openRootPasswordPrompt();
    void applyRootPassword(const QString &f_password);
    void openConfirmShutdown();
    void printStatus();

    void openTextEntry(const QString &f_title, const QString &f_prompt, bool f_masked,
                       std::function<void(const QString &)> f_submit);
    void submitTextEntry(const QString &f_text);

    ServerContext *m_server;
    // The primary menu holding the shared plugin task list; sessions point
    // at the menu they were created from, the primary points at itself.
    ConsoleMenu *m_action_source;
    std::function<void(const QByteArray &)> m_sink;
    bool m_interactive = false;
    bool m_vt = false;

    QString m_title;
    QString m_hint;
    QVector<Item> m_items;
    std::function<void()> m_back;
    int m_selected = 0;
    int m_rendered_lines = 0;

    bool m_text_entry = false;
    bool m_text_masked = false;
    QString m_text_prompt;
    QString m_text_input;
    std::function<void(const QString &)> m_text_submit;

    QList<ActionEntry> m_actions;
};

} // namespace akashi
