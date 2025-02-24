#include "medieval_parser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

inline int MedievalParser::randomInt(int min, int max)
{
    if (min > max) {
        return 0;
    }
    return QRandomGenerator::global()->bounded(min, max + 1);
}

MedievalParser::MedievalParser()
{
    parseDataFile();
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
        final_text.removeFirst();
    }

    return modifySpeech(final_text, do_pends, false);
}

void MedievalParser::parseDataFile()
{
    datafile_valid = true;

    QFile l_datafile_json("config/text/autorp.json");
    l_datafile_json.open(QIODevice::ReadOnly | QIODevice::Text);

    QJsonParseError l_error;
    const QJsonDocument &l_datafile_list_json = QJsonDocument::fromJson(l_datafile_json.readAll(), &l_error);
    if (!(l_error.error == QJsonParseError::NoError)) { // Non-Terminating error.
        qWarning() << "Unable to load Medieval Mode data file. The following error occurred: " + l_error.errorString();
        datafile_valid = false;
        return;
    }

    // Prepended words
    const QJsonObject &l_Json_prepend_object = l_datafile_list_json["prepended_words"].toObject();
    for (const QString &word : l_Json_prepend_object.keys()) {
        prepended_words.append(word);
    }

    if (prepended_words.isEmpty()) {
        datafile_valid = false;
        return;
    }

    // Appended words
    const QJsonObject &l_Json_append_object = l_datafile_list_json["appended_words"].toObject();
    for (const QString &word : l_Json_append_object.keys()) {
        appended_words.append(word);
    }

    if (appended_words.isEmpty()) {
        datafile_valid = false;
        return;
    }

    // Replaced words
    const QJsonArray &l_Json_replacement_array = l_datafile_list_json["word_replacements"].toArray();
    for (const QJsonValue &replacement : l_Json_replacement_array) {
        const QJsonObject &rep_obj = replacement.toObject();
        WordReplacement replacement_struct;
        for (const QString &key : rep_obj.keys()) {
            if (key == "replacement") {
                replacement_struct.replacements = QVector<QString>(rep_obj[key].toVariant().toStringList());
            }
            else if (key == "replacement_prepend") {
                replacement_struct.prepended = QVector<QString>(rep_obj[key].toVariant().toStringList());
            }
            else if (key == "replacement_plural") {
                replacement_struct.plural_replacements = QVector<QString>(rep_obj[key].toVariant().toStringList());
            }
            else if (key == "prepend_count") {
                replacement_struct.prepend_count = rep_obj[key].toVariant().toInt();
            }
            else if (key == "chance") {
                replacement_struct.chance = rep_obj[key].toVariant().toInt();
            }
            else if (key == "word") {
                replacement_struct.words = QVector<QString>(rep_obj[key].toVariant().toStringList());
                for (const QString &word : replacement_struct.words) {
                    word_vector.append(word);
                }
            }
            else if (key == "word_plural") {
                replacement_struct.plurals = QVector<QString>(rep_obj[key].toVariant().toStringList());
                for (const QString &word : replacement_struct.words) {
                    word_vector.append(word);
                }
            }
            else if (key == "prev") {
                replacement_struct.prev_words = QVector<QString>(rep_obj[key].toVariant().toStringList());
                for (const QString &word : replacement_struct.words) {
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

QString MedievalParser::getRandomPre()
{
    if (randomInt(1, 4) != 1) {
        return "";
    }
    if (prepended_words.isEmpty()) {
        return "";
    }

    static int prevPre = 0;
    prevPre += (randomInt(1, 4));
    while (prevPre >= prepended_words.count()) { // ensure we do not go out of bounds
        prevPre -= prepended_words.count();
    }

    return prepended_words[prevPre];
}

QString MedievalParser::getRandomPost()
{
    if (randomInt(1, 5) != 1) {
        return "";
    }
    if (appended_words.isEmpty()) {
        return "";
    }

    static int prevPost = 0;
    prevPost += randomInt(1, 4);
    while (prevPost >= appended_words.count()) { // ensure we do not go out of bounds
        prevPost -= appended_words.count();
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

    // if it has prewords make sure the preword matches first
    if (rep->prev_words.count() > 0) {
        if (check->prev_word.length() <= 0 || !word_vector.contains(check->prev_word, Qt::CaseInsensitive) || !rep->prev_words.contains(check->prev_word, Qt::CaseInsensitive)) {
            return MATCHES_NOT;
        }
        check->used_prev_word = true;
    }

    // check match type
    if (rep->words.contains(check->word, Qt::CaseInsensitive)) {
        return MATCHES_SINGULAR;
    }
    else if (rep->plurals.contains(check->word, Qt::CaseInsensitive)) {
        return MATCHES_PLURAL;
    }
    else {
        // no match, reset and return failure
        check->used_prev_word = false;
        return MATCHES_NOT;
    }
}

bool MedievalParser::replaceWord(ReplacementCheck *check, QString *rep_str, bool symbols, bool word_list_only)
{
    *rep_str = "";

    // First, see if we have a replacement
    for (auto &&replacement : word_replacements) {
        WordReplacement *rep_ptr = &replacement;
        MatchResult result = wordMatches(rep_ptr, check);
        if (result == MATCHES_NOT) {
            continue;
        }

        if (rep_ptr->prepended.count() > 0) {
            QVector<int> vector_used;
            for (int count = 0; count < rep_ptr->prepend_count; count++) {
                // Ensure we don't choose two of the same prepends
                int rnd = 0;
                do {
                    rnd = randomInt(0, rep_ptr->prepended.count());
                } while (vector_used.contains(rnd));
                vector_used.append(rnd);

                rep_str->append(rep_ptr->prepended[rnd]);
                if (count + 1 < rep_ptr->prepend_count) { // we have more prepends to prepend
                    rep_str->append(", ");
                }
                else {
                    rep_str->append(" ");
                }
            }
        }

        if (result == MATCHES_SINGULAR) {
            int rnd = randomInt(0, rep_ptr->replacements.count() - 1);
            rep_str->append(rep_ptr->replacements[rnd]);
        }
        else if (result == MATCHES_PLURAL) {
            int rnd = randomInt(0, rep_ptr->plural_replacements.count() - 1);
            rep_str->append(rep_ptr->plural_replacements[rnd]);
        }

        return true;
    }

    if (!symbols && !word_list_only) {
        QChar fc = check->word[0];

        // Randomly replace h's at the front of words with apostrophes
        if (fc == 'h' && randomInt(1, 2) == 1) {
            *rep_str = check->word.replace(0, 1, "'");
            return true;
        }

        QChar lc = check->word[check->word.length() - 1];
        if (check->word.length() > 3) {
            QChar slc = check->word[check->word.length() - 2];
            QChar lllc = check->word[check->word.length() - 3];

            // Randomly motify words ending in "ed", by replacing the "e" with an apostrophe
            if (slc == 'e' && lc == 'd' && lllc != 'e' && randomInt(1, 4) == 1) {
                *rep_str = check->word.replace(check->word.length() - 2, 1, "'");
                return true;
            }

            // Randomly append "th" or "st" to any word ending in "ke"
            if (slc == 'k' && lc == 'e' && randomInt(1, 3) == 1) {
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

        if (check->word.length() >= 3) {
            QChar slc = check->word[check->word.length() - 2];

            // Randomly append "eth" to words with appropriate last letters.
            if (randomInt(1, 5) == 1 &&
                (lc == 't' || lc == 'p' || lc == 'k' || lc == 'g' || lc == 'b' || lc == 'w')) {
                *rep_str = check->word;
                rep_str->append("eth");
                return true;
            }

            // Randomly append "est" to any word ending in "ss"
            if (lc == 's' && slc == 's' && randomInt(1, 5) == 1) {
                *rep_str = check->word;
                rep_str->append("est");
                return true;
            }
        }
        if (check->word.length() > 4) {
            // Randomly prepend "a-" to words ending in "ing", and randomly replace the trailing g with an apostrophe
            //		i.e. "coming" -> "a-comin'", "dancing" -> "a-dancing"
            QChar slc = check->word[check->word.length() - 2];
            QChar lllc = check->word[check->word.length() - 3];
            if (lllc == 'i' && slc == 'n' && lc == 'g') {
                QChar sc = check->word[2];
                if (sc != '-') {
                    rep_str->append("a-");

                    if (randomInt(1, 2) == 1) {
                        rep_str->append(check->word);
                    }
                    else {
                        rep_str->append(check->word.replace(check->word.length() - 1, 2, "' "));
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
        // Check to see if the previous word should be modified
        QChar fc = rep_str[0].toLower();
        // moderately cursed c_str comparison. +1 to len is for the null terminator
        if (!_strnicmp(check->prev_word.toStdString().c_str(), "an", qMax(check->prev_word.length() + 1, 2))) {
            if (fc != 'a' && fc != 'e' && fc != 'i' && fc != 'o' && fc != 'u') {
                // Remove the trailing n
                stored_word.removeLast();
                stored_word.removeLast();
                stored_word.append(' '); // 1 for space, 1 for n, then return space
            }
        }
        else if (check->prev_word == 'a') {
            if (fc == 'a' || fc == 'e' || fc == 'i' || fc == 'o' || fc == 'u') {
                // Add a trailing n
                stored_word.append("n");
            }
        }
    }

    // Only append the previous word if we didn't use it in our replacement
    if (!check->used_prev_word) {
        // Append the previous word
        out_text->append(stored_word);
        return true;
    }

    return false;
}

QString MedievalParser::modifySpeech(QString text, bool generate_pre_and_post, bool in_pre_post)
{
    QString final_text;
    if (generate_pre_and_post) {
        // See if we generate a pre. If we do, modify it as well so we can perform replacements on it.
        QString pre = getRandomPre();
        if (pre != "") {
            final_text.append(modifySpeech(pre, false, true) + " ");
        }
    }

    // Iterate through all words and test them against the replacement list
    // Note that a word is here defined as any group of letters separated
    // by any character other than &
    int prev_word_cur = 0;
    int current_word_cur = 0;
    int cur = 0;

    int text_len = text.length();

    QString stored_word = "";
    QString current_word = "";

    ReplacementCheck check;

    while (true) {
        if (cur > text_len) {
            break;
        }
        else if (cur == text_len) {
            text.append(" "); // needed since the old version depended on null terminators
        }
        else if ((text[cur] >= 'A' && text[cur] <= 'Z') || (text[cur] >= 'a' && text[cur] <= 'z') || text[cur] == '&') {
            cur++;
            continue;
        }

        // Not alphabetic or &. Hit the end of a word/string.
        int current_word_len = cur - current_word_cur;
        int prev_word_len = qMax(0, (int)(current_word_cur - prev_word_cur) - 1); // -1 for the space
        current_word = text.sliced(current_word_cur, current_word_len);
        check.prev_word = text.sliced(prev_word_cur, prev_word_len);
        check.used_prev_word = false;

        bool modify_word = true;
        bool skip_one_letter = false;

        // pre/post pend blocks only modify words that start with an '&'
        if (in_pre_post) {
            modify_word = (text[current_word_cur] == '&');
            skip_one_letter = modify_word;
        }

        if (skip_one_letter) {
            check.word = current_word.sliced(1, current_word.length() - 1);
        }
        else {
            check.word = current_word;
        }

        if (current_word_len > 0) {
            bool changed = modify_word ? replaceWord(&check, &current_word, false, in_pre_post) : false;

            // If the character that broke the last two words apart was an apostrophe, see if we can replace the whole word
            if (changed && modify_word) {
                if (!stored_word.isEmpty()) {
                    int st_len = stored_word.length();
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
                    // Append a space, but not if the last character is an apostrophe
                    int st_len = stored_word.length();
                    if (stored_word[st_len - 1] != '\'') {
                        final_text.append(" ");
                    }
                }
            }

            if (changed) {
                stored_word = current_word;

                // match case of the first letter in the word we're replacing
                if (text[current_word_cur] >= 'A' && text[current_word_cur] <= 'Z') {
                    stored_word[0] = stored_word[0].toUpper();
                }
                else if (text[current_word_cur] >= 'a' && text[current_word_cur] <= 'z') {
                    stored_word[0] = stored_word[0].toLower();
                }
            }
            else {
                stored_word = check.word;
            }
        }

        // Finished?
        if (cur == text_len) {
            check.used_prev_word = false;
            if (!stored_word.isEmpty()) {
                performReplacement("", &check, stored_word, &final_text);
            }
        }

        // If it wasn't a space that ended this word, try checking it for a symbol
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

        // Move on
        cur++;
        prev_word_cur = current_word_cur;
        current_word_cur = cur;
    }

    if (generate_pre_and_post) {
        if (!final_text.isEmpty()) {
            QChar pszLC = final_text[final_text.length() - 1];
            if (pszLC != '?' && pszLC != '!') {
                // See if we generate a post. If we do, modify it as well so we can perform replacements on it.
                QString post = getRandomPost();
                if (!post.isEmpty()) {
                    if (pszLC != '.') {
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
