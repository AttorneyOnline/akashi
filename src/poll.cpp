#include "poll.h"

#include <algorithm>

QString Poll::create(const QString &input)
{
    const QString text = input.trimmed();
    QString question;
    QString arguments;
    if (text.startsWith('"')) {
        const qsizetype end = text.indexOf('"', 1);
        if (end < 0)
            return "The poll question is missing its closing quote.";
        question = text.mid(1, end - 1).trimmed();
        arguments = text.mid(end + 1).trimmed();
    }
    else {
        question = text;
    }

    if (question.isEmpty())
        return "The poll question cannot be empty.";

    bool multipleChoice = false;
    QVector<Choice> choices;
    if (!arguments.isEmpty()) {
        const QString type = arguments.section(' ', 0, 0);
        const QString choiceText = arguments.mid(type.length()).trimmed();
        if (type.compare("MCQ", Qt::CaseInsensitive) == 0) {
            multipleChoice = true;
            if (choiceText.isEmpty())
                return "An MCQ poll needs at least one choice.";
            const QStringList options = choiceText.split(',');
            for (const QString &option : options) {
                const QString choice = option.trimmed();
                if (choice.isEmpty())
                    return "Poll choices cannot be empty.";
                for (const Choice &existing : choices) {
                    if (existing.text.compare(choice, Qt::CaseInsensitive) == 0)
                        return "Poll choices must be distinct.";
                }
                choices.append({choice, 0});
            }
        }
        else if (type.compare("FA", Qt::CaseInsensitive) != 0 || !choiceText.isEmpty()) {
            return "Use FA without choices, or MCQ followed by comma-separated choices.";
        }
    }

    m_question = question;
    m_multipleChoice = multipleChoice;
    m_choices = choices;
    m_votes.clear();
    return {};
}

QString Poll::vote(const QString &ipid, const QString &answer)
{
    if (!isActive())
        return "There is no active poll.";
    if (ipid.isEmpty())
        return "Your IPID is unavailable; your vote cannot be recorded.";
    if (m_votes.contains(ipid))
        return "You have already voted in this poll.";

    const QString text = answer.trimmed();
    if (text.isEmpty())
        return "Your answer cannot be empty.";

    int choiceIndex = -1;
    if (m_multipleChoice) {
        bool ok = false;
        const int number = text.toInt(&ok);
        if (ok && number >= 1 && number <= m_choices.size())
            choiceIndex = number - 1;
    }
    if (choiceIndex < 0) {
        for (int i = 0; i < m_choices.size(); ++i) {
            if (m_choices.at(i).text.compare(text, Qt::CaseInsensitive) == 0) {
                choiceIndex = i;
                break;
            }
        }
    }
    if (choiceIndex < 0) {
        if (m_multipleChoice)
            return "Choose a listed option by number or name.";
        choiceIndex = m_choices.size();
        m_choices.append({text, 0});
    }
    ++m_choices[choiceIndex].votes;
    m_votes.insert(ipid, choiceIndex);
    return {};
}

void Poll::clear()
{
    m_question.clear();
    m_multipleChoice = false;
    m_choices.clear();
    m_votes.clear();
}

bool Poll::isActive() const
{
    return !m_question.isEmpty();
}

QString Poll::display(bool results, bool global) const
{
    QStringList lines;
    lines.append(global ? "=== Global Poll ===" : "=== Area Poll ===");
    lines.append(m_question);
    if (results) {
        QVector<int> order;
        for (int i = 0; i < m_choices.size(); ++i)
            order.append(i);
        std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
            return m_choices.at(a).votes > m_choices.at(b).votes;
        });
        for (int index : order) {
            const Choice &choice = m_choices.at(index);
            const QString prefix = m_multipleChoice ? QString::number(index + 1) + ". " : "";
            lines.append(prefix + choice.text + " (" + QString::number(choice.votes) + ")");
        }
        lines.append("Total votes: " + QString::number(m_votes.size()));
    }
    else if (m_multipleChoice) {
        for (int i = 0; i < m_choices.size(); ++i)
            lines.append(QString::number(i + 1) + ". " + m_choices.at(i).text);
        lines.append(global ? "Vote with /gvote <number or choice>." : "Vote with /vote <number or choice>.");
    }
    else {
        lines.append(global ? "Vote with /gvote <answer>." : "Vote with /vote <answer>.");
    }
    lines.append("=============");
    return lines.join("\r\n");
}
