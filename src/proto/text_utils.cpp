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
QString sanitizePosition(QString f_position)
{
    return f_position.replace("../", "").replace("..\\", "");
}

} // namespace akashi
