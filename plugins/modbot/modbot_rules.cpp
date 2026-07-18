#include "modbot_rules.h"

namespace akashi::modbot {

Rules::Rules(const Config &f_config) :
    m_config(f_config)
{}

void Rules::prune(QList<qint64> &f_times, qint64 f_now, int f_window_seconds)
{
    while (!f_times.isEmpty() && f_times.first() <= f_now - f_window_seconds) {
        f_times.removeFirst();
    }
}

Verdict Rules::escalate(const Event &f_event, int f_prior_incidents, const QString &f_reason) const
{
    Verdict l_verdict;
    l_verdict.ipid = f_event.ipid;
    l_verdict.client_session_id = f_event.client_session_id;
    l_verdict.reason = f_reason;

    // The ladder: a first offender is warned, a repeat offender is muted
    // for longer each time, and a history at the threshold means a kick.
    if (f_prior_incidents >= m_config.kick_after_incidents) {
        l_verdict.action = Verdict::Action::Kick;
    }
    else if (f_prior_incidents >= 2) {
        l_verdict.action = Verdict::Action::Mute;
        l_verdict.mute_minutes = m_config.mute_minutes_repeat;
    }
    else if (f_prior_incidents >= 1) {
        l_verdict.action = Verdict::Action::Mute;
        l_verdict.mute_minutes = m_config.mute_minutes;
    }
    else {
        l_verdict.action = Verdict::Action::Warn;
    }
    return l_verdict;
}

QList<Verdict> Rules::analyze(const Event &f_event, int f_prior_incidents)
{
    // Human moderation events only feed the history; they never earn the
    // actor another verdict here.
    if (f_event.kind == Event::Kind::BanIssued || f_event.kind == Event::Kind::KickIssued) {
        return {};
    }
    if (f_event.ipid.isEmpty()) {
        return {};
    }

    ActorWindow &l_actor = m_actors[f_event.ipid];
    QList<Verdict> l_verdicts;

    // A fresh verdict already answered this burst; its tail neither earns
    // another one nor counts toward the next.
    if (f_event.epoch < l_actor.quiet_until) {
        return {};
    }

    switch (f_event.kind) {
    case Event::Kind::IcMessage:
    case Event::Kind::OocMessage:
    {
        l_actor.message_times.append(f_event.epoch);
        prune(l_actor.message_times, f_event.epoch, m_config.flood_seconds);

        const QString l_line = f_event.message.trimmed();
        if (!l_line.isEmpty() && l_line.compare(l_actor.last_message, Qt::CaseInsensitive) == 0) {
            l_actor.repeat_count++;
        }
        else {
            l_actor.last_message = l_line;
            l_actor.repeat_count = 1;
        }

        if (m_config.flood_messages > 0 && l_actor.message_times.size() >= m_config.flood_messages) {
            l_verdicts.append(escalate(f_event, f_prior_incidents, QStringLiteral("sending messages too quickly")));
            l_actor.message_times.clear();
            l_actor.quiet_until = f_event.epoch + m_config.flood_seconds;
        }
        else if (m_config.repeat_messages > 0 && l_actor.repeat_count >= m_config.repeat_messages) {
            l_verdicts.append(escalate(f_event, f_prior_incidents, QStringLiteral("repeating the same message")));
            l_actor.repeat_count = 0;
            l_actor.last_message.clear();
            l_actor.quiet_until = f_event.epoch + m_config.flood_seconds;
        }
        break;
    }
    case Event::Kind::ScreenDrop:
    {
        l_actor.drop_times.append(f_event.epoch);
        prune(l_actor.drop_times, f_event.epoch, m_config.strike_window_seconds);
        if (m_config.strike_screen_drops > 0 && l_actor.drop_times.size() >= m_config.strike_screen_drops) {
            l_verdicts.append(escalate(f_event, f_prior_incidents, QStringLiteral("posting blocked content")));
            l_actor.drop_times.clear();
            l_actor.quiet_until = f_event.epoch + m_config.strike_window_seconds;
        }
        break;
    }
    case Event::Kind::Modcall:
    {
        // Modcall floods only ever warn: punishing the help channel any
        // harder would teach people not to call moderators.
        l_actor.modcall_times.append(f_event.epoch);
        prune(l_actor.modcall_times, f_event.epoch, m_config.modcall_flood_seconds);
        if (m_config.modcall_flood_count > 0 && l_actor.modcall_times.size() >= m_config.modcall_flood_count) {
            Verdict l_verdict;
            l_verdict.action = Verdict::Action::Warn;
            l_verdict.ipid = f_event.ipid;
            l_verdict.client_session_id = f_event.client_session_id;
            l_verdict.reason = QStringLiteral("filing modcalls too quickly");
            l_verdicts.append(l_verdict);
            l_actor.modcall_times.clear();
            l_actor.quiet_until = f_event.epoch + m_config.modcall_flood_seconds;
        }
        break;
    }
    case Event::Kind::BanIssued:
    case Event::Kind::KickIssued:
        break;
    }

    return l_verdicts;
}

} // namespace akashi::modbot
