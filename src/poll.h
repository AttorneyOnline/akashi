#ifndef POLL_H
#define POLL_H

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class Poll
{
  public:
    QString create(const QString &input);
    QString vote(const QString &ipid, const QString &answer);
    void clear();
    bool isActive() const;
    QString display(bool results, bool global) const;

  private:
    struct Choice
    {
        QString text;
        int votes = 0;
    };

    QString m_question;
    bool m_multipleChoice = false;
    QVector<Choice> m_choices;
    QHash<QString, int> m_votes;
};

#endif // POLL_H
