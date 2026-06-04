#pragma once

#include <QObject>
#include <QString>
#include <QVector>

enum MatchResult
{
    MATCHES_NOT,
    MATCHES_SINGULAR,
    MATCHES_PLURAL
};

class MedievalParser
{
  public:
    explicit MedievalParser(const QString &f_config_path);
    explicit MedievalParser(const QByteArray &f_json_data);

    QString degrootify(QString message);

  private:
    void parseData(const QByteArray &f_json);

    struct WordReplacement
    {
        int chance = 1;
        int prepend_count = 0;
        QVector<QString> prepended;
        QVector<QString> replacements;
        QVector<QString> plural_replacements;
        QVector<QString> words;
        QVector<QString> plurals;
        QVector<QString> prev_words;
    };

    struct ReplacementCheck
    {
        QString word;
        int word_len;
        QString prev_word;
        int prev_word_len;
        bool used_prev_word;
    };

    QString randomPre();
    QString randomPost();
    QString modifySpeech(QString text, bool generate_pre_and_post, bool in_pre_post);
    MatchResult wordMatches(WordReplacement *rep, ReplacementCheck *check);
    bool replaceWord(ReplacementCheck *check, QString *rep, bool symbols, bool word_list_only);
    bool performReplacement(QString rep_str, ReplacementCheck *check, QString stored_word, QString *out_text);

    QVector<WordReplacement> word_replacements;
    QVector<QString> word_vector;
    QVector<QString> prepended_words;
    QVector<QString> appended_words;

    bool datafile_valid = false;

    int randomInt(int min, int max);
    bool containsCaseInsensitive(const QVector<QString> &vector, const QString &str);
};
