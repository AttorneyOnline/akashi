#ifndef AKASHI_AREA_RULE_H
#define AKASHI_AREA_RULE_H

#include <QString>

namespace akashi {

// The rule contract plugins build against. Its ABI stays stable across
// releases under these ground rules:
//  - AreaEvent only ever gains new values at the end; none are reordered
//    or removed.
//  - AreaEventDetails only ever gains new fields at the end. Core builds
//    it and rules receive it by reference, so older plugins keep reading
//    the fields they know.
//  - RuleVerdict's layout is frozen. New kinds of outcomes arrive as new
//    virtual methods on AreaRule, added at the end - existing methods are
//    never changed, reordered or removed.
// The plugin loader additionally refuses plugins built against a different
// compiler or Qt build, so the in-memory layout of these types agrees.

// The moments a rule can watch.
enum class AreaEvent
{
    PlayerJoined,
    PlayerLeft,
    MessageSent,
    EvidencePresented,
};

// What a rule gets to look at when its event happens.
struct AreaEventDetails
{
    int player_id = -1;
    int area_id = -1;
    int floor_id = -1;
    QString text; // the message or evidence name, when the event carries one
};

// What a rule decides about the event itself: let it happen, or block it
// with a reason the player gets to see. A rule's real work may be the
// actions it performs while running - the verdict only gates the event.
struct RuleVerdict
{
    bool allowed = true;
    QString reason;
};

// One rule, shippable by a plugin: subclass it, put the automation in
// onEvent, and register it with the rule registry under your plugin's
// owner id - unloading the plugin unregisters and releases it.
class AreaRule
{
  public:
    virtual ~AreaRule() = default;

    virtual RuleVerdict onEvent(const AreaEventDetails &f_details) = 0;
};

} // namespace akashi

#endif // AKASHI_AREA_RULE_H
