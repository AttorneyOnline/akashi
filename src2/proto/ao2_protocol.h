#ifndef AO2_PROTOCOL_H
#define AO2_PROTOCOL_H

// Named packet headers of the AO2 wire protocol, filled in as code moves over.
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
inline constexpr char HEADER_LE[] = "LE";

} // namespace ao2

#endif // AO2_PROTOCOL_H
