#pragma once

// Named packet headers of the AO2 network protocol, filled in as code moves over.
namespace ao2 {

// Client to server, in handshake order.
inline constexpr char HEADER_HI[] = "HI";
inline constexpr char HEADER_ID[] = "ID";
inline constexpr char HEADER_ASKCHAA[] = "askchaa";
inline constexpr char HEADER_RC[] = "RC";
inline constexpr char HEADER_RM[] = "RM";
inline constexpr char HEADER_RD[] = "RD";
inline constexpr char HEADER_CC[] = "CC";

// The chat family.
inline constexpr char HEADER_CT[] = "CT";
inline constexpr char HEADER_DE[] = "DE";
inline constexpr char HEADER_EE[] = "EE";
inline constexpr char HEADER_MS[] = "MS";
inline constexpr char HEADER_SETCASE[] = "SETCASE";

// The area and music family.
inline constexpr char HEADER_MC[] = "MC";
inline constexpr char HEADER_RT[] = "RT";
inline constexpr char HEADER_PE[] = "PE";

// The moderation and state-sync family.
inline constexpr char HEADER_ZZ[] = "ZZ";
inline constexpr char HEADER_MA[] = "MA";
inline constexpr char HEADER_CH[] = "CH";
inline constexpr char HEADER_CASEA[] = "CASEA";
inline constexpr char HEADER_PW[] = "PW";

// The dummy track 2.9+ clients use to stop the music.
inline constexpr char STOP_MUSIC[] = "~stop.mp3";

// Server to client.
inline constexpr char HEADER_BD[] = "BD";
inline constexpr char HEADER_PN[] = "PN";
inline constexpr char HEADER_FL[] = "FL";
inline constexpr char HEADER_ASS[] = "ASS";
inline constexpr char HEADER_SI[] = "SI";
inline constexpr char HEADER_SC[] = "SC";
inline constexpr char HEADER_SM[] = "SM";
inline constexpr char HEADER_HP[] = "HP";
inline constexpr char HEADER_FA[] = "FA";
inline constexpr char HEADER_DONE[] = "DONE";
inline constexpr char HEADER_BN[] = "BN";
inline constexpr char HEADER_TI[] = "TI";
inline constexpr char HEADER_KK[] = "KK";
inline constexpr char HEADER_KB[] = "KB";
inline constexpr char HEADER_LE[] = "LE";
inline constexpr char HEADER_CHECK[] = "CHECK";

// PR adds or removes a player list entry, PU updates one player's data.
inline constexpr char HEADER_PR[] = "PR";
inline constexpr char HEADER_PU[] = "PU";

enum PlayerListUpdate
{
    PLAYER_LIST_ADD = 0,
    PLAYER_LIST_REMOVE = 1,
};

enum PlayerDataType
{
    PLAYER_DATA_NAME = 0,
    PLAYER_DATA_CHARACTER = 1,
    PLAYER_DATA_CHARACTER_NAME = 2,
    PLAYER_DATA_AREA_ID = 3,
};

} // namespace ao2

