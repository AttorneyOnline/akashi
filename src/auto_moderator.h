#ifndef AUTO_MODERATOR_H
#define AUTO_MODERATOR_H

#include <QHash>
#include <QQueue>
#include <QString>

class AutoModerator
{
  public:
    enum class Trigger { None, Filter, Spam };

    Trigger observe(const QString &f_ipid, const QString &f_message, bool f_filter_match,
                    qint64 f_now, int f_window, int f_threshold, bool f_guarded);
    void clear();

  private:
    struct Message
    {
        QString text;
        qint64 time;
        bool filterMatch;
        bool spam;
        bool eligible;
    };

    struct State
    {
        QQueue<Message> messages;
        qint64 lastSeen = 0;
    };

    QHash<QString, State> m_states;
    qint64 m_last_cleanup = 0;
};

#endif
