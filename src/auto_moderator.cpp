#include "auto_moderator.h"

#include <QRegularExpression>
#include <QVector>
#include <algorithm>

namespace {
bool similar(const QString &f_first, const QString &f_second)
{
    if (f_first == f_second)
        return true;
    const qsizetype l_length = std::max(f_first.size(), f_second.size());
    if (l_length > 2048)
        return false;
    const int l_limit = static_cast<int>(l_length * 15 / 100);
    if (qAbs(f_first.size() - f_second.size()) > l_limit)
        return false;

    QVector<int> l_previous(f_second.size() + 1);
    QVector<int> l_current(f_second.size() + 1);
    for (qsizetype j = 0; j <= f_second.size(); ++j)
        l_previous[j] = static_cast<int>(j);
    for (qsizetype i = 1; i <= f_first.size(); ++i) {
        l_current[0] = static_cast<int>(i);
        int l_minimum = l_current[0];
        for (qsizetype j = 1; j <= f_second.size(); ++j) {
            l_current[j] = std::min({l_previous[j] + 1, l_current[j - 1] + 1,
                                     l_previous[j - 1] + (f_first[i - 1] != f_second[j - 1] ? 1 : 0)});
            l_minimum = std::min(l_minimum, l_current[j]);
        }
        if (l_minimum > l_limit)
            return false;
        l_previous.swap(l_current);
    }
    return l_previous[f_second.size()] <= l_limit;
}
}

AutoModerator::Trigger AutoModerator::observe(const QString &f_ipid, const QString &f_message, bool f_filter_match,
                                               qint64 f_now, int f_window, int f_threshold, bool f_guarded)
{
    if (f_now - m_last_cleanup >= 60) {
        for (auto it = m_states.begin(); it != m_states.end();) {
            if (f_now - it->lastSeen >= f_window)
                it = m_states.erase(it);
            else
                ++it;
        }
        m_last_cleanup = f_now;
    }

    State &l_state = m_states[f_ipid];
    l_state.lastSeen = f_now;
    while (!l_state.messages.isEmpty() && f_now - l_state.messages.head().time >= f_window)
        l_state.messages.dequeue();

    const QString l_normalized = f_message.simplified().toCaseFolded();
    static const QRegularExpression l_repeated("(.)\\1{14,}");
    static const QRegularExpression l_symbols("[^\\p{L}\\p{N}\\s]{15,}");
    bool l_spam = l_repeated.match(l_normalized).hasMatch() || l_symbols.match(l_normalized).hasMatch();
    if (!l_normalized.isEmpty()) {
        for (auto it = l_state.messages.rbegin(); it != l_state.messages.rend(); ++it) {
            if (similar(l_normalized, it->text)) {
                if (it->eligible)
                    it->spam = true;
                l_spam = true;
                break;
            }
        }
    }
    l_state.messages.enqueue({l_normalized, f_now, f_filter_match, l_spam, true});

    int l_filter_count = 0;
    int l_spam_count = 0;
    for (const Message &l_message : l_state.messages) {
        if (l_message.eligible) {
            l_filter_count += l_message.filterMatch ? 1 : 0;
            l_spam_count += l_message.spam ? 1 : 0;
        }
    }

    Trigger l_trigger = Trigger::None;
    if (f_filter_match && (f_guarded || l_filter_count >= f_threshold))
        l_trigger = Trigger::Filter;
    else if (l_spam && (f_guarded || l_spam_count >= f_threshold))
        l_trigger = Trigger::Spam;
    if (l_trigger != Trigger::None) {
        l_state.messages.clear();
        l_state.messages.enqueue({l_normalized, f_now, false, false, false});
    }
    return l_trigger;
}

void AutoModerator::clear()
{
    m_states.clear();
}
