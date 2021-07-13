#include <QMap>
#include <QString>

    /**
      * @brief The authorisation bitflag, representing what permissions a client can have.
      *
      * @showinitializer
      */
    QMap<QString, unsigned long long> ACLFlags {
        {"NONE",            0ULL      },
        {"KICK",            1ULL << 0 },
        {"BAN",             1ULL << 1 },
        {"BGLOCK",          1ULL << 2 },
        {"MODIFY_USERS",    1ULL << 3 },
        {"CM",              1ULL << 4 },
        {"GLOBAL_TIMER",    1ULL << 5 },
        {"EVI_MOD",         1ULL << 6 },
        {"MOTD",            1ULL << 7 },
        {"ANNOUNCE",        1ULL << 8 },
        {"MODCHAT",         1ULL << 9 },
        {"MUTE",            1ULL << 10},
        {"UNCM",            1ULL << 11},
        {"SAVETEST",        1ULL << 12},
        {"FORCE_CHARSELECT",1ULL << 13},
        {"BYPASS_LOCKS",    1ULL << 14},
        {"IGNORE_BGLIST",   1ULL << 15},
        {"SUPER",          ~0ULL      }
    };