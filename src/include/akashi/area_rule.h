#pragma once

#include <QString>
#include <QVariantMap>

#include <functional>

namespace akashi {

class ServiceRegistry;

// String event names. The enum-like namespace gives code aesthetics and
// autocomplete; internally everything dispatches by string.
namespace AreaEvents {
// Fired once per session in the starting area; player_joined fires on
// every area entry. A blocking before-rule refuses the connection.
inline const QString ServerJoined = QStringLiteral("server_joined");
inline const QString PlayerJoined = QStringLiteral("player_joined");
inline const QString PlayerLeft = QStringLiteral("player_left");
inline const QString IcMessageSent = QStringLiteral("ic_message_sent");
inline const QString OocMessageSent = QStringLiteral("ooc_message_sent");
inline const QString MusicChanged = QStringLiteral("music_changed");
inline const QString AmbienceChanged = QStringLiteral("ambience_changed");
inline const QString EvidencePresented = QStringLiteral("evidence_presented");
inline const QString EvidenceAdded = QStringLiteral("evidence_added");
inline const QString EvidenceRemoved = QStringLiteral("evidence_removed");
inline const QString EvidenceEdited = QStringLiteral("evidence_edited");
inline const QString BackgroundChanged = QStringLiteral("background_changed");
inline const QString LockChanged = QStringLiteral("lock_changed");
inline const QString OwnerChanged = QStringLiteral("owner_changed");
inline const QString CharacterChanged = QStringLiteral("character_changed");
} // namespace AreaEvents

enum class RulePhase
{
    Before,
    After,
};

// What a rule gets to look at when its event fires.
struct RuleContext
{
    int player_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QVariantMap payload;
    ServiceRegistry *services = nullptr;
};

// What a before-rule decides: let the event happen, or block it.
struct RuleVerdict
{
    bool allowed = true;
    QString reason;
};

using BeforeRuleFunction = std::function<RuleVerdict(const RuleContext &)>;
using AfterRuleFunction = std::function<void(const RuleContext &)>;

// An applied rule living on an area or floor. The arguments the function
// was built with travel along so the rule can be saved back to file.
struct BeforeRuleEntry
{
    QString event;
    QString action;
    BeforeRuleFunction function;
    QString owner_id;
    QVariantMap args;
};

struct AfterRuleEntry
{
    QString event;
    QString action;
    AfterRuleFunction function;
    QString owner_id;
    QVariantMap args;
};

// The rule contract plugins build against. Subclass for before-rules
// (return a verdict) or after-rules (act on the world).
class BeforeRule
{
  public:
    virtual ~BeforeRule() = default;

    virtual RuleVerdict onEvent(const RuleContext &f_context) = 0;
};

class AfterRule
{
  public:
    virtual ~AfterRule() = default;

    virtual void onEvent(const RuleContext &f_context) = 0;
};

} // namespace akashi
