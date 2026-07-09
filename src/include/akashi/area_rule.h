#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

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
inline const QString WtceUsed = QStringLiteral("wtce_used");
inline const QString StatusChanged = QStringLiteral("status_changed");
inline const QString DocChanged = QStringLiteral("doc_changed");
} // namespace AreaEvents

enum class RulePhase
{
    Before,
    After,
    // Appended: this SDK header is append-only.
    Transform,
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

// One core event's phase declaration for the catalog below.
struct AreaEventInfo
{
    QString id;
    bool supports_before = false;
    bool supports_after = false;
    // Appended fields: whether the transform phase dispatches, and whether
    // the event happens in an area (placed) or serverwide (placeless).
    bool supports_transform = false;
    bool placed = true;
};

// Every core event and the phases it dispatches. Ids not in this table
// are plugin/custom events and are not validated.
// supports_transform entries flip as each verb wires transform dispatch
// in; the IC verb dispatches it today. The placeless entries at the end
// are serverwide facts with no area: no rule phase dispatches, so
// /addrule refuses them, and only the global observers hear them.
inline const QVector<AreaEventInfo> &areaEventCatalog()
{
    static const QVector<AreaEventInfo> s_catalog = {
        {AreaEvents::ServerJoined, true, true},
        {AreaEvents::PlayerJoined, true, true},
        {AreaEvents::PlayerLeft, false, true},
        {AreaEvents::IcMessageSent, true, true, true},
        {AreaEvents::OocMessageSent, true, true},
        {AreaEvents::MusicChanged, true, true},
        {AreaEvents::AmbienceChanged, true, true},
        {AreaEvents::EvidencePresented, true, true},
        {AreaEvents::EvidenceAdded, true, true},
        {AreaEvents::EvidenceRemoved, true, true},
        {AreaEvents::EvidenceEdited, true, true},
        {AreaEvents::BackgroundChanged, true, true},
        {AreaEvents::LockChanged, true, true},
        {AreaEvents::OwnerChanged, true, true},
        {AreaEvents::CharacterChanged, true, true},
        {AreaEvents::WtceUsed, true, true},
        {AreaEvents::StatusChanged, true, true},
        {AreaEvents::DocChanged, true, true},
        // Placeless events, observe-only.
        {QStringLiteral("modcall"), false, false, false, false},
        {QStringLiteral("ban_issued"), false, false, false, false},
        {QStringLiteral("kick_issued"), false, false, false, false},
        {QStringLiteral("player_disconnected"), false, false, false, false},
        {QStringLiteral("config_reloaded"), false, false, false, false},
    };
    return s_catalog;
}

// Everything below is appended: this SDK header is append-only.

// A transform rule rewrites parts of the event payload. Transforms run
// between the before gate and the commit, the area's before the floor's
// non-overridden ones, and each sees the payload as its predecessors
// left it. A transform returns ONLY the keys it changed; an empty map
// means no change.
using TransformRuleFunction = std::function<QVariantMap(const RuleContext &)>;

struct TransformRuleEntry
{
    QString event;
    QString action;
    TransformRuleFunction function;
    QString owner_id;
    QVariantMap args;
};

// A global observer watches an event after it committed. Observers never
// gate or rewrite anything. Main thread only, like all rule dispatch.
using ObserverFunction = std::function<void(const RuleContext &)>;

} // namespace akashi
