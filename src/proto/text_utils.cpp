#include "proto/text_utils.h"

#include <QRegularExpression>

namespace akashi {

QString stripZalgo(QString f_text)
{
    static const QRegularExpression l_combining_marks("([̴̵̶̷̸̡̢̧̨̛̖̗̘̙̜̝̞̟̠̣̤̥̦̩̪̫̬̭̮̯̰̱̲̳̹̺̻̼͇͈͉͍͎̀́̂̃̄̅̆̇̈̉̊̋̌̍̎̏̐̑̒̓̔̽̾̿̀́͂̓̈́͆͊͋͌̕̚ͅ͏͓͔͕͖͙͚͐͑͒͗͛ͣͤͥͦͧͨͩͪͫͬͭͮͯ͘͜͟͢͝͞͠͡])");
    return f_text.replace(l_combining_marks, "");
}

// A position is a client-supplied folder name; traversal sequences are
// stripped so it can never step outside the theme's side folders. The one
// sanitizer for every position write and every position echo.
//
// Stripping is repeated until the string stops changing: a single pass lets a
// crafted input like "....//" collapse back into "../", so the traversal
// survives. Looping removes the reconstructed sequence too.
QString sanitizePosition(QString f_position)
{
    QString l_previous;
    do {
        l_previous = f_position;
        f_position.replace("../", "").replace("..\\", "");
    } while (f_position != l_previous);
    return f_position;
}

} // namespace akashi
