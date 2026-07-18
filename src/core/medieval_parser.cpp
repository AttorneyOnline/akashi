#include "medieval_parser.h"

#include "akashi/logging_categories.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include <algorithm>

inline int MedievalParser::randomInt(int min, int max)
{
    if (min > max) {
        return 0;
    }
    return QRandomGenerator::global()->bounded(min, max + 1);
}

inline bool MedievalParser::containsCaseInsensitive(const QVector<QString> &vector, const QString &str)
{
    return vector.contains(str, Qt::CaseInsensitive);
}

MedievalParser::MedievalParser(const QString &f_config_path)
{
    QFile l_datafile_json(f_config_path);
    if (!l_datafile_json.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(akashiPlugins) << "Unable to open the Medieval Mode data file.";
        datafile_valid = false;
        return;
    }
    parseData(l_datafile_json.readAll());
}

MedievalParser::MedievalParser(const QByteArray &f_json_data)
{
    parseData(f_json_data);
}

QString MedievalParser::degrootify(QString message)
{
    if (!datafile_valid) {
        return message;
    }
    bool do_pends = true;
    QString final_text = message;

    if (message.startsWith("-")) {
        do_pends = false;
        final_text.remove(0, 1);
    }

    return modifySpeech(final_text, do_pends, false);
}

void MedievalParser::parseData(const QByteArray &f_json)
{
    datafile_valid = true;

    QJsonParseError l_error;
    const QJsonDocument &l_datafile_list_json = QJsonDocument::fromJson(f_json, &l_error);
    if (!(l_error.error == QJsonParseError::NoError)) {
        qCWarning(akashiPlugins) << "Unable to load Medieval Mode data file. The following error occurred: " + l_error.errorString();
        datafile_valid = false;
        return;
    }

    const QJsonObject &l_Json_prepend_object = l_datafile_list_json["prepended_words"].toObject();
    for (const QString &word : l_Json_prepend_object.keys()) {
        prepended_words.append(word);
    }

    if (prepended_words.isEmpty()) {
        datafile_valid = false;
        return;
    }

    const QJsonObject &l_Json_append_object = l_datafile_list_json["appended_words"].toObject();
    for (const QString &word : l_Json_append_object.keys()) {
        appended_words.append(word);
    }

    if (appended_words.isEmpty()) {
        datafile_valid = false;
        return;
    }

    const QJsonArray &l_Json_replacement_array = l_datafile_list_json["word_replacements"].toArray();
    for (const QJsonValue &replacement : l_Json_replacement_array) {
        const QJsonObject &rep_obj = replacement.toObject();
        WordReplacement replacement_struct;
        for (const QString &key : rep_obj.keys()) {
            if (key == "replacement") {
                replacement_struct.replacements = QVector<QString>(rep_obj[key].toVariant().toStringList().toVector());
            }
            else if (key == "replacement_prepend") {
                replacement_struct.prepended = QVector<QString>(rep_obj[key].toVariant().toStringList().toVector());
            }
            else if (key == "replacement_plural") {
                replacement_struct.plural_replacements = QVector<QString>(rep_obj[key].toVariant().toStringList().toVector());
            }
            else if (key == "prepend_count") {
                replacement_struct.prepend_count = rep_obj[key].toVariant().toInt();
            }
            else if (key == "chance") {
                replacement_struct.chance = rep_obj[key].toVariant().toInt();
            }
            else if (key == "word") {
                replacement_struct.words = QVector<QString>(rep_obj[key].toVariant().toStringList().toVector());
                for (const QString &word : replacement_struct.words) {
                    word_vector.append(word);
                }
            }
            else if (key == "word_plural") {
                replacement_struct.plurals = QVector<QString>(rep_obj[key].toVariant().toStringList().toVector());
                for (const QString &word : replacement_struct.plurals) {
                    word_vector.append(word);
                }
            }
            else if (key == "prev") {
                replacement_struct.prev_words = QVector<QString>(rep_obj[key].toVariant().toStringList().toVector());
                for (const QString &word : replacement_struct.prev_words) {
                    word_vector.append(word);
                }
            }
        }
        word_replacements.append(replacement_struct);
    }
    if (word_replacements.isEmpty()) {
        datafile_valid = false;
        return;
    }
}

QString MedievalParser::randomPre()
{
    if (randomInt(1, 4) != 1) {
        return "";
    }
    if (prepended_words.isEmpty()) {
        return "";
    }

    static int prevPre = 0;
    prevPre += (randomInt(1, 4));
    while (prevPre >= prepended_words.size()) {
        prevPre -= prepended_words.size();
    }

    return prepended_words[prevPre];
}

QString MedievalParser::randomPost()
{
    if (randomInt(1, 5) != 1) {
        return "";
    }
    if (appended_words.isEmpty()) {
        return "";
    }

    static int prevPost = 0;
    prevPost += randomInt(1, 4);
    while (prevPost >= appended_words.size()) {
        prevPost -= appended_words.size();
    }

    return appended_words[prevPost];
}

MatchResult MedievalParser::wordMatches(WordReplacement *rep, ReplacementCheck *check)
{
    if (rep->chance != 1) {
        if (randomInt(1, rep->chance) > 1) {
            return MATCHES_NOT;
        }
    }

    if (rep->prev_words.size() > 0) {
        if (check->prev_word.size() <= 0 || !containsCaseInsensitive(word_vector, check->prev_word) || !containsCaseInsensitive(rep->prev_words, check->prev_word)) {
            return MATCHES_NOT;
        }
        check->used_prev_word = true;
    }

    if (containsCaseInsensitive(rep->words, check->word)) {
        return MATCHES_SINGULAR;
    }
    else if (containsCaseInsensitive(rep->plurals, check->word)) {
        return MATCHES_PLURAL;
    }
    else {
        check->used_prev_word = false;
        return MATCHES_NOT;
    }
}

bool MedievalParser::replaceWord(ReplacementCheck *check, QString *rep_str, bool symbols, bool word_list_only)
{
    *rep_str = "";

    for (auto &&replacement : word_replacements) {
        WordReplacement *rep_ptr = &replacement;
        MatchResult result = wordMatches(rep_ptr, check);
        if (result == MATCHES_NOT) {
            continue;
        }

        const QVector<QString> &l_pool = (result == MATCHES_PLURAL && !rep_ptr->plural_replacements.isEmpty())
                                             ? rep_ptr->plural_replacements
                                             : rep_ptr->replacements;
        if (l_pool.isEmpty()) {
            check->used_prev_word = false;
            continue;
        }

        if (rep_ptr->prepended.size() > 0) {
            QVector<int> vector_used;
            const int l_prepend_count = std::min(rep_ptr->prepend_count, int(rep_ptr->prepended.size()));
            for (int count = 0; count < l_prepend_count; count++) {
                int rnd = 0;
                do {
                    rnd = randomInt(0, rep_ptr->prepended.size() - 1);
                } while (vector_used.contains(rnd));
                vector_used.append(rnd);

                rep_str->append(rep_ptr->prepended[rnd]);
                if (count + 1 < l_prepend_count) {
                    rep_str->append(", ");
                }
                else {
                    rep_str->append(" ");
                }
            }
        }

        rep_str->append(l_pool[randomInt(0, l_pool.size() - 1)]);

        return true;
    }

    if (!symbols && !word_list_only) {
        const char16_t fc = check->word[0].unicode();

        if (fc == u'h' && randomInt(1, 2) == 1) {
            *rep_str = check->word.replace(0, 1, "'");
            return true;
        }

        const char16_t lc = check->word[check->word.size() - 1].unicode();
        if (check->word.size() > 3) {
            const char16_t slc = check->word[check->word.size() - 2].unicode();
            const char16_t lllc = check->word[check->word.size() - 3].unicode();

            if (slc == u'e' && lc == u'd' && lllc != u'e' && randomInt(1, 4) == 1) {
                *rep_str = check->word.replace(check->word.size() - 2, 1, "'");
                return true;
            }

            if (slc == u'k' && lc == u'e' && randomInt(1, 3) == 1) {
                *rep_str = check->word;
                if (randomInt(1, 2) == 1) {
                    rep_str->append("th");
                }
                else {
                    rep_str->append("st");
                }
                return true;
            }
        }

        if (check->word.size() >= 3) {
            const char16_t slc = check->word[check->word.size() - 2].unicode();

            if (randomInt(1, 5) == 1 &&
                (lc == u't' || lc == u'p' || lc == u'k' || lc == u'g' || lc == u'b' || lc == u'w')) {
                *rep_str = check->word;
                rep_str->append("eth");
                return true;
            }

            if (lc == u's' && slc == u's' && randomInt(1, 5) == 1) {
                *rep_str = check->word;
                rep_str->append("est");
                return true;
            }
        }
        if (check->word.size() > 4) {
            const char16_t slc = check->word[check->word.size() - 2].unicode();
            const char16_t lllc = check->word[check->word.size() - 3].unicode();
            if (lllc == u'i' && slc == u'n' && lc == u'g') {
                const char16_t sc = check->word[2].unicode();
                if (sc != u'-') {
                    rep_str->append("a-");

                    if (randomInt(1, 2) == 1) {
                        rep_str->append(check->word);
                    }
                    else {
                        rep_str->append(check->word.replace(check->word.size() - 1, 2, "' "));
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

bool MedievalParser::performReplacement(QString rep_str, ReplacementCheck *check, QString stored_word, QString *out_text)
{
    if (!rep_str.isEmpty()) {
        const char16_t fc = rep_str[0].toLower().unicode();
        if (!qstrnicmp(check->prev_word.toStdString().c_str(), "an", std::max<qsizetype>(check->prev_word.size() + 1, 2))) {
            if (fc != u'a' && fc != u'e' && fc != u'i' && fc != u'o' && fc != u'u') {
                stored_word.chop(2);
                stored_word.append(' ');
            }
        }
        else if (check->prev_word == 'a') {
            if (fc == u'a' || fc == u'e' || fc == u'i' || fc == u'o' || fc == u'u') {
                stored_word.append("n");
            }
        }
    }

    if (!check->used_prev_word) {
        out_text->append(stored_word);
        return true;
    }

    return false;
}

QString MedievalParser::modifySpeech(QString text, bool generate_pre_and_post, bool in_pre_post)
{
    QString final_text;
    if (generate_pre_and_post) {
        QString pre = randomPre();
        if (pre != "") {
            final_text.append(modifySpeech(pre, false, true) + " ");
        }
    }

    int prev_word_cur = 0;
    int current_word_cur = 0;
    int cur = 0;

    int text_len = text.size();

    QString stored_word = "";
    QString current_word = "";

    ReplacementCheck check;

    while (true) {
        if (cur > text_len) {
            break;
        }
        else if (cur == text_len) {
            text.append(" ");
        }
        else if ((text[cur] >= 'A' && text[cur] <= 'Z') || (text[cur] >= 'a' && text[cur] <= 'z') || text[cur] == '&') {
            cur++;
            continue;
        }

        int current_word_len = cur - current_word_cur;
        int prev_word_len = std::max(0, (int)(current_word_cur - prev_word_cur) - 1);
        current_word = text.mid(current_word_cur, current_word_len);
        check.prev_word = text.mid(prev_word_cur, prev_word_len);
        check.used_prev_word = false;

        bool modify_word = true;
        bool skip_one_letter = false;

        if (in_pre_post) {
            modify_word = (text[current_word_cur] == '&');
            skip_one_letter = modify_word;
        }

        if (skip_one_letter) {
            check.word = current_word.mid(1, current_word.size() - 1);
        }
        else {
            check.word = current_word;
        }

        if (current_word_len > 0) {
            bool changed = modify_word ? replaceWord(&check, &current_word, false, in_pre_post) : false;

            if (changed && modify_word) {
                if (!stored_word.isEmpty()) {
                    int st_len = stored_word.size();
                    if (stored_word[st_len - 1] == '\'') {
                        check.word = stored_word;
                        check.word.append(current_word);
                        check.prev_word = "";

                        changed = replaceWord(&check, &current_word, false, in_pre_post);
                        if (changed) {
                            check.used_prev_word = true;
                        }
                    }
                }
            }

            if (!stored_word.isEmpty()) {
                if (performReplacement(current_word, &check, stored_word, &final_text)) {
                    int st_len = stored_word.size();
                    if (stored_word[st_len - 1] != '\'') {
                        final_text.append(" ");
                    }
                }
            }

            if (changed) {
                stored_word = current_word;

                // An empty replacement drops the word entirely, leaving no
                // first letter whose case could be matched.
                if (!stored_word.isEmpty()) {
                    if (text[current_word_cur] >= 'A' && text[current_word_cur] <= 'Z') {
                        stored_word[0] = stored_word[0].toUpper();
                    }
                    else if (text[current_word_cur] >= 'a' && text[current_word_cur] <= 'z') {
                        stored_word[0] = stored_word[0].toLower();
                    }
                }
            }
            else {
                stored_word = check.word;
            }
        }

        if (cur == text_len) {
            check.used_prev_word = false;
            if (!stored_word.isEmpty()) {
                performReplacement("", &check, stored_word, &final_text);
            }
        }

        if (text[cur] != ' ') {
            check.word = text[cur];
            check.used_prev_word = false;

            QString symbol_rep = "";

            if (replaceWord(&check, &symbol_rep, true, true)) {
                stored_word.append(symbol_rep);
            }
            else {
                stored_word.append(text[cur]);
            }
        }

        cur++;
        prev_word_cur = current_word_cur;
        current_word_cur = cur;
    }

    if (generate_pre_and_post) {
        if (!final_text.isEmpty()) {
            const char16_t pszLC = final_text[final_text.size() - 1].unicode();
            if (pszLC != u'?' && pszLC != u'!') {
                QString post = randomPost();
                if (!post.isEmpty()) {
                    if (pszLC != u'.') {
                        final_text.append(". ");
                    }
                    else {
                        final_text.append(" ");
                    }

                    final_text.append(modifySpeech(post, false, true));
                }
            }
        }
    }

    return final_text;
}
