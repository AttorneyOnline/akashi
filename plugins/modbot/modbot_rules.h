#pragma once

#include <QHash>
#include <QList>
#include <QMetaType>
#include <QRegularExpression>
#include <QString>

namespace akashi::modbot {

// Everything the rules are allowed to know, read from the plugin config
// once at load.
struct Config
{
    // The pre-echo screen: a message matching any pattern never reaches
    // other players.
    QList<QRegularExpression> screen_patterns;

    // Sending this many messages inside the window is a flood.
    int flood_messages = 8;
    int flood_seconds = 5;

    // Saying the same line this many times in a row is spam.
    int repeat_messages = 4;

    // This many screened messages inside the window earns a verdict.
    int strike_screen_drops = 3;
    int strike_window_seconds = 60;

    // Filing this many modcalls inside the window earns a warning.
    int modcall_flood_count = 3;
    int modcall_flood_seconds = 30;

    // The escalation ladder: warn first, then these mute lengths, then a
    // kick once the incident history reaches the threshold.
    int mute_minutes = 5;
    int mute_minutes_repeat = 30;
    int kick_after_incidents = 4;
};

// One snapshot of something that happened, self-contained so it can cross
// into the analysis thread: ids and copied strings, never live pointers.
struct Event
{
    enum class Kind
    {
        IcMessage,
        OocMessage,
        ScreenDrop,
        Modcall,
        BanIssued,
        KickIssued,
    };

    Kind kind = Kind::IcMessage;
    qint64 epoch = 0; // seconds
    int client_session_id = -1;
    QString ipid;
    QString character;
    QString ooc_name;
    QString area_name;
    QString message;
};

// What the analysis decided; executed on the main thread, gated by the
// bot's ACL role there.
struct Verdict
{
    enum class Action
    {
        Warn,
        Mute,
        Kick,
    };

    Action action = Action::Warn;
    QString ipid;
    int client_session_id = -1;
    QString reason;
    int mute_minutes = 0;
};

// The analysis rules, free of threads and services so the tests drive
// them with plain events and synthetic clocks. One instance belongs to
// the worker thread; the sliding windows live per ipid.
class Rules
{
  public:
    explicit Rules(const Config &f_config);

    // Feeds one event. f_prior_incidents is the actor's count from the
    // bot's own incident store and drives the escalation ladder.
    QList<Verdict> analyze(const Event &f_event, int f_prior_incidents);

  private:
    struct ActorWindow
    {
        QList<qint64> message_times;
        QList<qint64> drop_times;
        QList<qint64> modcall_times;
        QString last_message;
        int repeat_count = 0;
        // No further verdicts for the actor until this moment, so one
        // burst answers with one verdict instead of one per message.
        qint64 quiet_until = 0;
    };

    Verdict escalate(const Event &f_event, int f_prior_incidents, const QString &f_reason) const;
    static void prune(QList<qint64> &f_times, qint64 f_now, int f_window_seconds);

    Config m_config;
    QHash<QString, ActorWindow> m_actors;
};

} // namespace akashi::modbot

Q_DECLARE_METATYPE(akashi::modbot::Verdict)
