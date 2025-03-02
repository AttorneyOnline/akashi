#ifndef MEDIEVAL_PARSER_H
#define MEDIEVAL_PARSER_H
#pragma once

#include <QObject>
#include <QString>
///
/// Medieval text parser, reimplemented from tf_autorp in the Source 1 SDK 2013
/// Please do not report bugs found in this parser without confirming they do not also exist in TF2
///

enum MatchResult
{
    MATCHES_NOT,
    MATCHES_SINGULAR,
    MATCHES_PLURAL
};

class MedievalParser
{
  public:
    MedievalParser();

    QString degrootify(QString message);

  private:
    void parseDataFile();

    struct WordReplacement
    {
        int chance;
        int prepend_count;
        QVector<QString> prepended;           // Words that prepend the replacement
        QVector<QString> replacements;        // Words that replace the original word
        QVector<QString> plural_replacements; // If the match was a plural match, use these replacements instead, if they exist
        QVector<QString> words;               // Word that matches this replacement
        QVector<QString> plurals;             // Word that must come before to match this replacement, for double word replacements, i.e. "it is" -> "'tis"
        QVector<QString> prev_words;          // same ^^
    };

    struct ReplacementCheck
    {
        QString word;
        int word_len;
        QString prev_word;
        int prev_word_len;
        bool used_prev_word;
    };

    QString getRandomPre();
    QString getRandomPost();
    QString modifySpeech(QString text, bool generate_pre_and_post, bool in_pre_post);
    MatchResult wordMatches(WordReplacement *rep, ReplacementCheck *check);
    bool replaceWord(ReplacementCheck *check, QString *rep, bool symbols, bool word_list_only);
    bool performReplacement(QString rep_str, ReplacementCheck *check, QString stored_word, QString *out_text);

    QVector<WordReplacement> word_replacements;

    QVector<QString> word_vector;

    QVector<QString> prepended_words;
    QVector<QString> appended_words;

    bool datafile_valid;

    int randomInt(int min, int max);
    bool containsCaseInsensitive(const QVector<QString> &vector, const QString &str);
};

#endif // MEDIEVAL_PARSER_H
