#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <algorithm>

// The permission ids core declares. Commands gate on them, role files
// grant them, and rule arguments name them. Plugins declare their own ids
// through the PermissionRegistry service and reference them as plain
// strings; these constants only exist so core never mistypes its own.
//
// The naming law: one act, one name, spelled <domain>.<verb> with the
// verb players actually type whenever a command fronts the act - area.cm
// IS the /cm permission. Legacy spellings (gamemaster, mute, ...) map
// through the roles loader's alias table.
namespace akashi::permission {
inline const QString none = QStringLiteral("none");
inline const QString kick = QStringLiteral("kick");
inline const QString ban = QStringLiteral("ban");
inline const QString lock_background = QStringLiteral("lock_background");
inline const QString modify_users = QStringLiteral("modify_users");
inline const QString area_cm = QStringLiteral("area.cm");
inline const QString timer_global = QStringLiteral("timer.global");
inline const QString modify_evidence = QStringLiteral("modify_evidence");
inline const QString motd = QStringLiteral("motd");
inline const QString announcer = QStringLiteral("announcer");
inline const QString chat_moderator = QStringLiteral("chat_moderator");
inline const QString area_uncm = QStringLiteral("area.uncm");
inline const QString save_testimony = QStringLiteral("save_testimony");
inline const QString force_charselect = QStringLiteral("force_charselect");
inline const QString area_enter = QStringLiteral("area.enter");
inline const QString ignore_background_list = QStringLiteral("ignore_background_list");
inline const QString send_notice = QStringLiteral("send_notice");
inline const QString music_jukebox = QStringLiteral("music.jukebox");
inline const QString modify_rules = QStringLiteral("modify_rules");
inline const QString modify_floors = QStringLiteral("modify_floors");
inline const QString super = QStringLiteral("super");
inline const QString user = QStringLiteral("user");
// The per-sanction administration permissions, one per verb - the old
// blanket "mute" expands to all nine through the alias table, and a role
// may now hold any subset.
inline const QString sanction_mute = QStringLiteral("sanction.mute");
inline const QString sanction_ooc_mute = QStringLiteral("sanction.ooc_mute");
inline const QString sanction_block_dj = QStringLiteral("sanction.block_dj");
inline const QString sanction_block_wtce = QStringLiteral("sanction.block_wtce");
inline const QString sanction_gimp = QStringLiteral("sanction.gimp");
inline const QString sanction_disemvowel = QStringLiteral("sanction.disemvowel");
inline const QString sanction_shake = QStringLiteral("sanction.shake");
inline const QString sanction_medieval = QStringLiteral("sanction.medieval");
inline const QString sanction_charcurse = QStringLiteral("sanction.charcurse");
// The formerly unnamed privileges: acts that used to hide behind a bare
// "is logged in" check and could be neither granted nor revoked.
inline const QString see_ipids = QStringLiteral("see_ipids");
inline const QString receive_modcalls = QStringLiteral("receive_modcalls");
inline const QString see_staff_presence = QStringLiteral("see_staff_presence");
inline const QString see_real_names = QStringLiteral("see_real_names");
inline const QString change_locked_background = QStringLiteral("change_locked_background");
// The everyone-baseline acts a joined person does by existing; the block
// sanctions are masks over exactly these.
inline const QString ic_chat = QStringLiteral("ic_chat");
inline const QString ooc_chat = QStringLiteral("ooc_chat");
inline const QString change_music = QStringLiteral("change_music");
inline const QString use_wtce = QStringLiteral("use_wtce");
// Holders cannot be sanctioned by anyone below super - checked where a
// sanction is applied, so it covers transforms and charcurse too.
inline const QString sanction_immune = QStringLiteral("sanction.immune");
// The ordinary operations: each everyday verb is its own permission, so
// an owner can grant or drop any single act - a whitelist server may
// close all of them. The everyone section in permissions.json defines
// who holds them; defaultBaseline() below is what applies when no
// section exists. Only the login-and-account door stays on user.
inline const QString music_play = QStringLiteral("music.play");
inline const QString music_play_ambience = QStringLiteral("music.play_ambience");
inline const QString music_currentmusic = QStringLiteral("music.currentmusic");
inline const QString messaging_g = QStringLiteral("messaging.g");
inline const QString messaging_need = QStringLiteral("messaging.need");
inline const QString messaging_pm = QStringLiteral("messaging.pm");
inline const QString messaging_a = QStringLiteral("messaging.a");
inline const QString messaging_s = QStringLiteral("messaging.s");
inline const QString roleplay_coinflip = QStringLiteral("roleplay.coinflip");
inline const QString roleplay_roll = QStringLiteral("roleplay.roll");
inline const QString roleplay_rolla = QStringLiteral("roleplay.rolla");
inline const QString roleplay_rollp = QStringLiteral("roleplay.rollp");
inline const QString roleplay_notecard = QStringLiteral("roleplay.notecard");
inline const QString roleplay_notecard_clear = QStringLiteral("roleplay.notecard_clear");
inline const QString roleplay_8ball = QStringLiteral("roleplay.8ball");
inline const QString casing_doc = QStringLiteral("casing.doc");
inline const QString casing_cleardoc = QStringLiteral("casing.cleardoc");
inline const QString casing_testimony = QStringLiteral("casing.testimony");
inline const QString area_background = QStringLiteral("area.background");
inline const QString area_status = QStringLiteral("area.status");
inline const QString area_webfiles = QStringLiteral("area.webfiles");
inline const QString area_area = QStringLiteral("area.area");
inline const QString area_floor = QStringLiteral("area.floor");
inline const QString area_floors = QStringLiteral("area.floors");
inline const QString area_getarea = QStringLiteral("area.getarea");
inline const QString area_getareas = QStringLiteral("area.getareas");
inline const QString characters_switch = QStringLiteral("characters.switch");
inline const QString characters_randomchar = QStringLiteral("characters.randomchar");
inline const QString characters_charselect = QStringLiteral("characters.charselect");
inline const QString characters_pos = QStringLiteral("characters.pos");
inline const QString messaging_afk = QStringLiteral("messaging.afk");
inline const QString messaging_firstperson = QStringLiteral("messaging.firstperson");
inline const QString messaging_toggleglobal = QStringLiteral("messaging.toggleglobal");
inline const QString messaging_mutepm = QStringLiteral("messaging.mutepm");
inline const QString messaging_toggleadverts = QStringLiteral("messaging.toggleadverts");
inline const QString info_help = QStringLiteral("info.help");
inline const QString info_commands = QStringLiteral("info.commands");
inline const QString info_why = QStringLiteral("info.why");
inline const QString info_motd = QStringLiteral("info.motd");
inline const QString info_mods = QStringLiteral("info.mods");
inline const QString info_about = QStringLiteral("info.about");
inline const QString info_rules = QStringLiteral("info.rules");
inline const QString info_ruleactions = QStringLiteral("info.ruleactions");

// The case-manager powers: each CM command is its own cm.<verb>
// permission, so the whole set forms a configurable role. area.cm stays
// the CM *status* (evidence sight, free-play bypass, target immunity, the
// claim); these are the *powers*. The area_owner grant source hands out
// area.cm plus this bundle - or the @cm group's members, if one is
// declared - to an area's owners, so a server can restrict or extend what
// a CM may do by editing @cm.
inline const QString cm_invite = QStringLiteral("cm.invite");
inline const QString cm_uninvite = QStringLiteral("cm.uninvite");
inline const QString cm_lock = QStringLiteral("cm.lock");
inline const QString cm_spectate = QStringLiteral("cm.spectate");
inline const QString cm_unlock = QStringLiteral("cm.unlock");
inline const QString cm_kick = QStringLiteral("cm.kick");
inline const QString cm_side = QStringLiteral("cm.side");
inline const QString cm_lock_background = QStringLiteral("cm.lock_background");
inline const QString cm_unlock_background = QStringLiteral("cm.unlock_background");
inline const QString cm_judgelog = QStringLiteral("cm.judgelog");
inline const QString cm_areamessage = QStringLiteral("cm.areamessage");
inline const QString cm_togglemessage = QStringLiteral("cm.togglemessage");
inline const QString cm_clearmessage = QStringLiteral("cm.clearmessage");
inline const QString cm_toggle_wtce = QStringLiteral("cm.toggle_wtce");
inline const QString cm_toggle_shouts = QStringLiteral("cm.toggle_shouts");
inline const QString cm_togglemusic = QStringLiteral("cm.togglemusic");
inline const QString cm_addmusic = QStringLiteral("cm.addmusic");
inline const QString cm_addmusiccategory = QStringLiteral("cm.addmusiccategory");
inline const QString cm_removecustommusic = QStringLiteral("cm.removecustommusic");
inline const QString cm_togglecustommusic = QStringLiteral("cm.togglecustommusic");
inline const QString cm_clearcustommusic = QStringLiteral("cm.clearcustommusic");
inline const QString cm_jukebox_skip = QStringLiteral("cm.jukebox_skip");
inline const QString cm_evidence_swap = QStringLiteral("cm.evidence_swap");
inline const QString cm_testify = QStringLiteral("cm.testify");
inline const QString cm_examine = QStringLiteral("cm.examine");
inline const QString cm_delete = QStringLiteral("cm.delete");
inline const QString cm_update = QStringLiteral("cm.update");
inline const QString cm_pause = QStringLiteral("cm.pause");
inline const QString cm_add = QStringLiteral("cm.add");
inline const QString cm_loadtestimony = QStringLiteral("cm.loadtestimony");
inline const QString cm_forcepos = QStringLiteral("cm.forcepos");
inline const QString cm_timer = QStringLiteral("cm.timer");
inline const QString cm_notecard_reveal = QStringLiteral("cm.notecard_reveal");
inline const QString cm_subtheme = QStringLiteral("cm.subtheme");
inline const QString cm_force_noint_pres = QStringLiteral("cm.force_noint_pres");
inline const QString cm_allow_iniswap = QStringLiteral("cm.allow_iniswap");

// The stock case-manager powers, granted at area scope to an area's
// owners when permissions.json declares no @cm group. music.jukebox rides
// along so a CM may toggle the area jukebox by default; drop it from @cm
// to take that away. area.cm (the status) is granted separately and is
// not listed here.
inline QStringList defaultCmPowers()
{
    return {cm_invite, cm_uninvite, cm_lock, cm_spectate, cm_unlock, cm_kick,
            cm_side, cm_lock_background, cm_unlock_background, cm_judgelog,
            cm_areamessage, cm_togglemessage, cm_clearmessage, cm_toggle_wtce,
            cm_toggle_shouts, cm_togglemusic, cm_addmusic, cm_addmusiccategory,
            cm_removecustommusic, cm_togglecustommusic, cm_clearcustommusic,
            cm_jukebox_skip, cm_evidence_swap, cm_testify, cm_examine, cm_delete,
            cm_update, cm_pause, cm_add, cm_loadtestimony, cm_forcepos, cm_timer,
            cm_notecard_reveal, cm_subtheme, cm_force_noint_pres, cm_allow_iniswap,
            music_jukebox};
}

// One permission as core declares it. The catalog below is the single
// list: registration walks it, the everyone-baseline is the rows that
// say so, and the everyone-offer guard is the restricted column. A new
// core permission is one row here and nothing else.
struct CatalogEntry
{
    QString id;
    QString display_name;
    QString category;
    // Restricted acts never ride an everyone-shaped offer, whatever the
    // scope - a config typo must not hand a room the mute or the IPID
    // radar. They are role grants, or they are nothing.
    bool restricted = false;
    // Part of the stock everyone-baseline: what a joined person may do on
    // a server whose permissions.json has no everyone section.
    bool in_baseline = false;
};

inline const QList<CatalogEntry> &catalog()
{
    static const QList<CatalogEntry> s_catalog = [] {
        QList<CatalogEntry> l_entries{
            {kick, QStringLiteral("Kick"), QStringLiteral("moderation"), true, false},
            {ban, QStringLiteral("Ban"), QStringLiteral("moderation"), true, false},
            {lock_background, QStringLiteral("Lock Background"), QStringLiteral("area"), false, false},
            {modify_users, QStringLiteral("Modify Users"), QStringLiteral("administration"), true, false},
            {area_cm, QStringLiteral("Case Manager"), QStringLiteral("area"), false, false},
            {timer_global, QStringLiteral("Global Timer"), QStringLiteral("area"), true, false},
            {modify_evidence, QStringLiteral("Modify Evidence"), QStringLiteral("area"), false, false},
            {motd, QStringLiteral("MOTD"), QStringLiteral("administration"), false, false},
            {announcer, QStringLiteral("Announcer"), QStringLiteral("moderation"), false, false},
            {chat_moderator, QStringLiteral("Chat Moderator"), QStringLiteral("moderation"), true, false},
            {sanction_mute, QStringLiteral("Mute"), QStringLiteral("moderation"), true, false},
            {sanction_ooc_mute, QStringLiteral("OOC Mute"), QStringLiteral("moderation"), true, false},
            {sanction_block_dj, QStringLiteral("Block DJ"), QStringLiteral("moderation"), true, false},
            {sanction_block_wtce, QStringLiteral("Block Judge Controls"), QStringLiteral("moderation"), true, false},
            {sanction_gimp, QStringLiteral("Gimp"), QStringLiteral("moderation"), true, false},
            {sanction_disemvowel, QStringLiteral("Disemvowel"), QStringLiteral("moderation"), true, false},
            {sanction_shake, QStringLiteral("Shake"), QStringLiteral("moderation"), true, false},
            {sanction_medieval, QStringLiteral("Medieval"), QStringLiteral("moderation"), true, false},
            {sanction_charcurse, QStringLiteral("Charcurse"), QStringLiteral("moderation"), true, false},
            {area_uncm, QStringLiteral("Remove CM"), QStringLiteral("moderation"), true, false},
            {save_testimony, QStringLiteral("Save Testimony"), QStringLiteral("area"), false, false},
            {force_charselect, QStringLiteral("Force Charselect"), QStringLiteral("moderation"), true, false},
            {area_enter, QStringLiteral("Enter Locked Areas"), QStringLiteral("moderation"), true, false},
            {ignore_background_list, QStringLiteral("Ignore BG List"), QStringLiteral("area"), false, false},
            {send_notice, QStringLiteral("Send Notice"), QStringLiteral("moderation"), false, false},
            {music_jukebox, QStringLiteral("Jukebox"), QStringLiteral("area"), false, false},
            {modify_rules, QStringLiteral("Modify Rules"), QStringLiteral("area"), true, false},
            {modify_floors, QStringLiteral("Modify Floors"), QStringLiteral("administration"), true, false},
            {super, QStringLiteral("Super"), QStringLiteral("administration"), true, false},
            {user, QStringLiteral("User"), QStringLiteral("lifecycle"), false, false},
            {see_ipids, QStringLiteral("See IPIDs"), QStringLiteral("moderation"), true, false},
            {receive_modcalls, QStringLiteral("Receive Modcalls"), QStringLiteral("moderation"), true, false},
            {see_staff_presence, QStringLiteral("See Staff Presence"), QStringLiteral("moderation"), true, false},
            {see_real_names, QStringLiteral("See Real Names"), QStringLiteral("moderation"), true, false},
            {change_locked_background, QStringLiteral("Change Locked Background"), QStringLiteral("area"), false, false},
            {ic_chat, QStringLiteral("IC Chat"), QStringLiteral("speech"), false, true},
            {ooc_chat, QStringLiteral("OOC Chat"), QStringLiteral("speech"), false, true},
            {change_music, QStringLiteral("Change Music"), QStringLiteral("area"), false, true},
            {use_wtce, QStringLiteral("Use Judge Controls"), QStringLiteral("area"), false, true},
            {sanction_immune, QStringLiteral("Sanction Immunity"), QStringLiteral("moderation"), true, false},
            {music_play, QStringLiteral("Play Music"), QStringLiteral("music"), false, true},
            {music_play_ambience, QStringLiteral("Play Ambience"), QStringLiteral("music"), false, true},
            {music_currentmusic, QStringLiteral("Show Current Music"), QStringLiteral("music"), false, true},
            {messaging_g, QStringLiteral("Global Chat"), QStringLiteral("messaging"), false, true},
            {messaging_need, QStringLiteral("Send Adverts"), QStringLiteral("messaging"), false, true},
            {messaging_pm, QStringLiteral("Private Messages"), QStringLiteral("messaging"), false, true},
            {messaging_a, QStringLiteral("Message Owned Area"), QStringLiteral("messaging"), false, true},
            {messaging_s, QStringLiteral("Message Owned Areas"), QStringLiteral("messaging"), false, true},
            {roleplay_coinflip, QStringLiteral("Coin Flip"), QStringLiteral("roleplay"), false, true},
            {roleplay_roll, QStringLiteral("Roll Dice"), QStringLiteral("roleplay"), false, true},
            {roleplay_rolla, QStringLiteral("Roll Named Die"), QStringLiteral("roleplay"), false, true},
            {roleplay_rollp, QStringLiteral("Roll Privately"), QStringLiteral("roleplay"), false, true},
            {roleplay_notecard, QStringLiteral("Write Notecard"), QStringLiteral("roleplay"), false, true},
            {roleplay_notecard_clear, QStringLiteral("Clear Notecard"), QStringLiteral("roleplay"), false, true},
            {roleplay_8ball, QStringLiteral("Magic 8-Ball"), QStringLiteral("roleplay"), false, true},
            {casing_doc, QStringLiteral("Document"), QStringLiteral("casing"), false, true},
            {casing_cleardoc, QStringLiteral("Clear Document"), QStringLiteral("casing"), false, true},
            {casing_testimony, QStringLiteral("View Testimony"), QStringLiteral("casing"), false, true},
            {area_background, QStringLiteral("Change Background"), QStringLiteral("area"), false, true},
            {area_status, QStringLiteral("Change Status"), QStringLiteral("area"), false, true},
            {area_webfiles, QStringLiteral("List Webfiles"), QStringLiteral("area"), false, true},
            {area_area, QStringLiteral("Move To Area"), QStringLiteral("area"), false, true},
            {area_floor, QStringLiteral("Move To Floor"), QStringLiteral("area"), false, true},
            {area_floors, QStringLiteral("List Floors"), QStringLiteral("area"), false, true},
            {area_getarea, QStringLiteral("List Area Clients"), QStringLiteral("area"), false, true},
            {area_getareas, QStringLiteral("List All Clients"), QStringLiteral("area"), false, true},
            {characters_switch, QStringLiteral("Switch Character"), QStringLiteral("characters"), false, true},
            {characters_randomchar, QStringLiteral("Random Character"), QStringLiteral("characters"), false, true},
            {characters_charselect, QStringLiteral("Character Select"), QStringLiteral("characters"), false, true},
            {characters_pos, QStringLiteral("Change Position"), QStringLiteral("characters"), false, true},
            {messaging_afk, QStringLiteral("AFK"), QStringLiteral("messaging"), false, true},
            {messaging_firstperson, QStringLiteral("First-Person Mode"), QStringLiteral("messaging"), false, true},
            {messaging_toggleglobal, QStringLiteral("Toggle Global Chat"), QStringLiteral("messaging"), false, true},
            {messaging_mutepm, QStringLiteral("Mute PMs"), QStringLiteral("messaging"), false, true},
            {messaging_toggleadverts, QStringLiteral("Toggle Adverts"), QStringLiteral("messaging"), false, true},
            {info_help, QStringLiteral("Help"), QStringLiteral("info"), false, true},
            {info_commands, QStringLiteral("List Commands"), QStringLiteral("info"), false, true},
            {info_why, QStringLiteral("Explain Permissions"), QStringLiteral("info"), false, true},
            {info_motd, QStringLiteral("View MOTD"), QStringLiteral("info"), false, true},
            {info_mods, QStringLiteral("List Moderators"), QStringLiteral("info"), false, true},
            {info_about, QStringLiteral("About"), QStringLiteral("info"), false, true},
            {info_rules, QStringLiteral("View Rules"), QStringLiteral("info"), false, true},
            {info_ruleactions, QStringLiteral("List Rule Actions"), QStringLiteral("info"), false, true},
        };
        // The case-manager powers all share one shape, so they are grown
        // from the bundle instead of typed out again. music.jukebox rides
        // in that bundle and is already spelled out above, so it is
        // skipped rather than declared twice.
        for (const QString &l_power : defaultCmPowers()) {
            const bool l_already = std::any_of(l_entries.cbegin(), l_entries.cend(),
                                               [&l_power](const CatalogEntry &f_entry) { return f_entry.id == l_power; });
            if (!l_already) {
                l_entries.append({l_power, QStringLiteral("CM: ") + l_power.section(QLatin1Char('.'), 1),
                                  QStringLiteral("cm"), false, false});
            }
        }
        return l_entries;
    }();
    return s_catalog;
}

// The stock everyone-baseline: what a joined person may do on a server
// whose permissions.json has no everyone section. Everything here is
// droppable, the speech-and-play baseline included - a whitelist server
// closes the lot and grants per role or per area. Only user itself, the
// login-and-account commands and packet dispatch stay core-granted, so
// authenticating always works.
inline QStringList defaultBaseline()
{
    QStringList l_names;
    for (const CatalogEntry &l_entry : catalog()) {
        if (l_entry.in_baseline) {
            l_names << l_entry.id;
        }
    }
    return l_names;
}

// The legacy spellings and what they became - the one table every sink
// translates through: role files, group members, rule bypass arguments
// and grant declarations. The old blanket "mute" expands to every
// per-sanction administration permission. Warns at the sink for one
// release; names beyond this table are the quarantine's business.
inline const QHash<QString, QStringList> &legacyAliases()
{
    static const QHash<QString, QStringList> s_aliases = [] {
        // A role that granted "gamemaster" meant "acts as a CM": under the
        // split that is the status plus the whole power bundle, so the old
        // spelling still confers full CM ability wherever it was granted.
        QStringList l_gamemaster{area_cm};
        l_gamemaster += defaultCmPowers();
        return QHash<QString, QStringList>{
            {QStringLiteral("gamemaster"), l_gamemaster},
            {QStringLiteral("remove_gamemaster"), {area_uncm}},
            {QStringLiteral("global_timer"), {timer_global}},
            {QStringLiteral("jukebox"), {music_jukebox}},
            {QStringLiteral("bypass_locks"), {area_enter}},
            // The retired blanket's surviving function is lock passage.
            {QStringLiteral("bypass_rules"), {area_enter}},
            {QStringLiteral("sanction_immune"), {sanction_immune}},
            {QStringLiteral("mute"),
             {sanction_mute, sanction_ooc_mute, sanction_block_dj, sanction_block_wtce,
              sanction_gimp, sanction_disemvowel, sanction_shake, sanction_medieval,
              sanction_charcurse}},
        };
    }();
    return s_aliases;
}
} // namespace akashi::permission
