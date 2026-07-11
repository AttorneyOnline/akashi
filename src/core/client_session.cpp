#include "core/client_session.h"

#include "akashi/authenticator.h"
#include "akashi/config_store.h"
#include "akashi/event.h"
#include "akashi/log_event.h"
#include "akashi/logging_categories.h"
#include "akashi/sanctions.h"
#include "core/arup_broadcaster.h"
#include "core/auth_throttle.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/db_manager.h"
#include "core/log_service.h"
#include "core/permission_registry.h"
#include "core/player_state_observer.h"
#include "core/server_context.h"
#include "core/server_settings.h"
#include "core/text_filter_registry.h"
#include "proto/ao2_protocol.h"
#include "proto/evidence.h"
#include "proto/ic.h"
#include "proto/packet.h"
#include "proto/text_utils.h"
#include "world/area.h"
#include "world/jukebox.h"
#include "world/rule_registry.h"

#include <QPointer>
#include <QQueue>

#include <functional>

// How many outbound packets a session holds while its connection is down. Bounded
// so a session nobody deletes cannot hoard memory; a full buffer drops the
// oldest packet and records the overflow.
static const int PENDING_PACKET_LIMIT = 512;

namespace akashi {

void ClientSession::waitForReconnect(int f_grace_seconds)
{
    waiting_for_reconnect = true;
    reconnect_timer->start(f_grace_seconds * 1000);
}

void ClientSession::cancelReconnectWait()
{
    waiting_for_reconnect = false;
    reconnect_timer->stop();
}

PlayerState *ClientSession::addPlayer(int f_id, int f_limit)
{
    if (players.size() >= f_limit) {
        return nullptr;
    }
    PlayerState *l_player = new PlayerState(f_id, this);
    players.append(l_player);
    return l_player;
}

void ClientSession::write(const Packet &f_packet)
{
    // Outbound interceptors see every packet on its way to this client -
    // the seam for per-recipient rewrites - and may drop it.
    Packet l_packet = f_packet;
    if (packets && !packets->outboundInterceptors().intercept(l_packet, *this)) {
        return;
    }

    if (transport->isOpen()) {
        transport->write(l_packet);
        return;
    }

    if (pending_packets.size() >= PENDING_PACKET_LIMIT) {
        pending_packets.dequeue();
        pending_overflowed = true;
    }
    pending_packets.enqueue(l_packet);
}

void ClientSession::bindTransport(ITransport *f_transport)
{
    if (transport) {
        disconnect(transport, nullptr, this, nullptr);
        transport->deleteLater();
    }
    if (waiting_for_reconnect) {
        cancelReconnectWait();
    }

    transport = f_transport;
    transport->setParent(this);
    remote_ip = transport->peerAddress();

    // The capabilities the client announced while connecting, the header
    // side of the FL exchange. In the profile before ID resolves codecs,
    // so codec rules can key on them.
    const QStringList l_announced = transport->connectTimeFeatures();
    for (const QString &l_feature : l_announced) {
        m_profile.features.insert(l_feature);
    }
    connect(transport, &ITransport::packetReceived, this, &ClientSession::packetReceived);
    connect(transport, &ITransport::clientDisconnected, this, &ClientSession::transportClosed);

    while (!pending_packets.isEmpty() && transport->isOpen()) {
        transport->write(pending_packets.dequeue());
    }
}

void ClientSession::leave()
{
    if (m_left) {
        return;
    }
    m_left = true;
#ifdef NET_DEBUG
    qCDebug(akashiNet) << remote_ip.toString() << "disconnected";
#endif
    // A client that never joined has no presence to withdraw - and a rejected
    // (banned) connection must not broadcast ARUPs to the whole server.
    // A server-less test session has no world to leave.
    if (m_stage == SessionStage::Joined && m_server) {
        m_server->areaById(areaId())->removeClient(m_server->characterId(character()), clientId());
        runAfterRule(akashi::AreaEvents::PlayerLeft, {});

        if (character() != "") {
            m_server->updateCharsTaken(m_server->areaById(areaId()));
        }

        const QVector<akashi::Area *> l_areas = m_server->areas();
        for (akashi::Area *l_area : l_areas) {
            if (l_area->invited().contains(id)) {
                l_area->uninvite(id);
            }

            l_area->removeOwner(clientId());
        }
    }
}

void ClientSession::handlePacket(const akashi::Packet &packet)
{
#ifdef NET_DEBUG
    qCDebug(akashiNet) << "Received packet:" << packet.header() << ":" << packet.fields() << "args length:" << packet.fieldCount();
#endif

    // A session without a server (a test harness) forwards packets through
    // its signal but does not process the game.
    if (!m_server) {
        return;
    }

    qint64 current_tick = QDateTime::currentSecsSinceEpoch();
    if (rate_limit_tick < current_tick) {
        rate_limit_tick = current_tick;
        packet_count = 0;
    }

    ++packet_count;
    int hard_limit = m_server->serverSettings()->packet_rate_limit_hard();
    int soft_limit = m_server->serverSettings()->packet_rate_limit_soft();

    if (hard_limit > 0 && packet_count >= hard_limit) {
        sendPacket("BD", {"You have been disconnected for sending messages too quickly."});
        transport->close();
        return;
    }
    else if (soft_limit > 0 && packet_count >= soft_limit) {
        sendServerMessage("You are sending messages too quickly. Please slow down.");
    }

    // Unreadable data still counts against the rate limit above.
    if (packet.isNull()) {
        return;
    }

    if (packet.fields().join("").size() > 16384) {
        return;
    }

    if (packets) {
        if (const auto l_spec = packets->handlers().spec(packet.header())) {
            handleRegisteredPacket(packet, *l_spec);
        }
    }
    // A header nobody registered is dropped.
}

void ClientSession::handleRegisteredPacket(const akashi::Packet &f_packet, const akashi::PacketSpec &f_spec)
{
    if (!canPerform(f_spec.required_permission)) {
        return;
    }

    resetAfk(f_packet.header());

    if (f_packet.fieldCount() < f_spec.min_args) {
        return;
    }

    const std::shared_ptr<akashi::Codec> l_codec = codecs.codecFor(f_packet.header());
    if (!l_codec) {
        return;
    }
    const std::unique_ptr<akashi::Message> l_message = l_codec->decode(f_packet);
    if (!l_message) {
        return;
    }
    packets->handlers().handler(f_packet.header())->handle(*l_message, *this);
}

void ClientSession::resetAfk(const QString &f_header)
{
    if (f_header != "CH" && m_stage == SessionStage::Joined) {
        if (afk) {
            sendServerMessage("You are no longer AFK.");
        }
        afk = false;
        if (characterName().endsWith(" [AFK]")) {
            setCharacterName(characterName().remove(" [AFK]"));
        }
        afk_timer->start(m_server->serverSettings()->afk_timeout() * 1000);
    }
}

void ClientSession::changeArea(int new_area)
{
    if (areaId() == new_area) {
        sendServerMessage("You are already in area " + m_server->areaName(areaId()));
        return;
    }

    int l_old_area = areaId();
    int l_old_floor = m_server->floorIdForArea(l_old_area);
    int l_new_floor = m_server->floorIdForArea(new_area);

    akashi::Area *l_target = m_server->areaById(new_area);
    akashi::RuleContext l_ctx;
    l_ctx.player_state_id = player()->id();
    l_ctx.client_session_id = clientId();
    l_ctx.area_id = new_area;
    l_ctx.floor_id = l_new_floor;
    l_ctx.services = m_server->services();
    l_ctx.payload = withActorIdentity({
        {QStringLiteral("lock_status"),
         l_target->lockState() == akashi::Area::LockState::Locked ? QStringLiteral("locked") : l_target->lockState() == akashi::Area::LockState::Spectatable ? QStringLiteral("spectatable")
                                                                                                                                                             : QStringLiteral("free")},
        {QStringLiteral("is_invited"), l_target->invited().contains(clientId())},
        {QStringLiteral("bypass_locks"), canPerform(permission::bypass_locks)},
        {QStringLiteral("area_name"), m_server->areaName(new_area)},
        {QStringLiteral("character_id"), m_server->characterId(character())},
    });

    // Same floor as checkBeforeRule: bypass_rules holders pass every
    // before-rule, including the target area's join gate.
    const akashi::Floor *l_target_floor = m_server->floorById(l_new_floor);
    if (!canPerform(permission::bypass_rules)) {
        akashi::RuleVerdict l_verdict = akashi::RuleRegistry::checkBefore(
            akashi::AreaEvents::PlayerJoined, l_ctx,
            l_target->beforeRules(),
            l_target_floor ? l_target_floor->before_rules : QVector<akashi::BeforeRuleEntry>{});
        if (!l_verdict.allowed) {
            sendServerMessage(l_verdict.reason);
            return;
        }
    }

    if (character() != "") {
        m_server->areaById(areaId())->changeCharacter(m_server->characterId(character()), -1);
        m_server->updateCharsTaken(m_server->areaById(areaId()));
    }
    m_server->areaById(areaId())->removeClient(player()->char_id, clientId());
    // The pair request was scoped to the scene it left behind.
    setPairingWith(-1);
    bool l_character_taken = false;
    if (m_server->areaById(new_area)->charactersTaken().contains(m_server->characterId(character()))) {
        setCharacter("");
        player()->char_id = -1;
        l_character_taken = true;
    }
    m_server->areaById(new_area)->addClient(player()->char_id, clientId());
    setAreaId(new_area);
    if (l_character_taken) {
        sendPacket("DONE");
    }
    sendServerMessage("You moved to area " + m_server->areaName(areaId()));

    // The area's state (evidence, penalties, background, timers, music,
    // floor map) reaches the client through the player_joined after-rules.
    runAfterRule(akashi::AreaEvents::PlayerJoined, {
                                                       {QStringLiteral("from_area"), l_old_area},
                                                       {QStringLiteral("from_floor"), l_old_floor},
                                                       {QStringLiteral("character_taken"), l_character_taken},
                                                   });
}

int ClientSession::floorCount() const
{
    return m_server->floorCount();
}

int ClientSession::floorAreaId(int f_floor_id, int f_x) const
{
    const akashi::Floor *l_floor = m_server->floorById(f_floor_id);
    if (!l_floor || f_x < 0 || f_x >= l_floor->area_ids.size())
        return -1;
    return l_floor->area_ids[f_x];
}

QStringList ClientSession::floorAreaNames() const
{
    const akashi::Floor *l_floor = m_server->floorById(m_server->floorIdForArea(areaId()));
    if (!l_floor)
        return m_server->areaNames();
    QStringList l_names;
    for (int l_aid : l_floor->area_ids) {
        l_names.append(m_server->areaName(l_aid));
    }
    return l_names;
}

int ClientSession::floorAreaToGlobal(int f_local_index) const
{
    const akashi::Floor *l_floor = m_server->floorById(m_server->floorIdForArea(areaId()));
    if (!l_floor || f_local_index < 0 || f_local_index >= l_floor->area_ids.size())
        return -1;
    return l_floor->area_ids[f_local_index];
}

void stampActorIdentity(QVariantMap &f_payload, int f_client_session_id, int f_player_state_id,
                        const QString &f_char_name, const QString &f_ooc_name)
{
    if (!f_payload.contains(QStringLiteral("client_session_id"))) {
        f_payload.insert(QStringLiteral("client_session_id"), f_client_session_id);
    }
    if (!f_payload.contains(QStringLiteral("player_state_id"))) {
        f_payload.insert(QStringLiteral("player_state_id"), f_player_state_id);
    }
    if (!f_payload.contains(QStringLiteral("char_name"))) {
        f_payload.insert(QStringLiteral("char_name"), f_char_name);
    }
    if (!f_payload.contains(QStringLiteral("ooc_name"))) {
        f_payload.insert(QStringLiteral("ooc_name"), f_ooc_name);
    }
}

QVariantMap ClientSession::withActorIdentity(QVariantMap f_payload) const
{
    stampActorIdentity(f_payload, clientId(), player()->id(), player()->character(), player()->oocName());
    return f_payload;
}

std::optional<QString> ClientSession::checkBeforeRule(const QString &f_event, const QVariantMap &f_payload)
{
    // Rules only tighten for ordinary players; a bypass_rules holder is
    // never gated by any before-rule.
    if (canPerform(permission::bypass_rules)) {
        return std::nullopt;
    }

    akashi::RuleContext l_ctx;
    l_ctx.player_state_id = player()->id();
    l_ctx.client_session_id = clientId();
    l_ctx.area_id = areaId();
    l_ctx.floor_id = m_server->floorIdForArea(areaId());
    l_ctx.services = m_server->services();
    l_ctx.payload = withActorIdentity(f_payload);
    akashi::Area *l_area = m_server->areaById(areaId());
    const akashi::Floor *l_floor = m_server->floorById(l_ctx.floor_id);
    akashi::RuleVerdict l_verdict = akashi::RuleRegistry::checkBefore(f_event, l_ctx,
                                                                      l_area ? l_area->beforeRules() : QVector<akashi::BeforeRuleEntry>{},
                                                                      l_floor ? l_floor->before_rules : QVector<akashi::BeforeRuleEntry>{});
    if (!l_verdict.allowed)
        return l_verdict.reason;
    return std::nullopt;
}

QVariantMap ClientSession::runTransformRules(const QString &f_event, const QVariantMap &f_payload)
{
    // No bypass_rules floor here: transforms are area flavor, not gates -
    // a moderator's message still gets medieval-ized.
    akashi::RuleContext l_ctx;
    l_ctx.player_state_id = player()->id();
    l_ctx.client_session_id = clientId();
    l_ctx.area_id = areaId();
    l_ctx.floor_id = m_server->floorIdForArea(areaId());
    l_ctx.services = m_server->services();
    l_ctx.payload = withActorIdentity(f_payload);
    akashi::Area *l_area = m_server->areaById(areaId());
    const akashi::Floor *l_floor = m_server->floorById(l_ctx.floor_id);
    return akashi::RuleRegistry::runTransforms(f_event, l_ctx,
                                               l_area ? l_area->transformRules() : QVector<akashi::TransformRuleEntry>{},
                                               l_floor ? l_floor->transform_rules : QVector<akashi::TransformRuleEntry>{});
}

void ClientSession::runAfterRule(const QString &f_event, const QVariantMap &f_payload)
{
    akashi::RuleContext l_ctx;
    l_ctx.player_state_id = player()->id();
    l_ctx.client_session_id = clientId();
    l_ctx.area_id = areaId();
    l_ctx.floor_id = m_server->floorIdForArea(areaId());
    l_ctx.services = m_server->services();
    l_ctx.payload = withActorIdentity(f_payload);
    akashi::Area *l_area = m_server->areaById(areaId());
    const akashi::Floor *l_floor = m_server->floorById(l_ctx.floor_id);
    akashi::RuleRegistry::runAfter(f_event, l_ctx,
                                   l_area ? l_area->afterRules() : QVector<akashi::AfterRuleEntry>{},
                                   l_floor ? l_floor->after_rules : QVector<akashi::AfterRuleEntry>{});
    // The committed fact reaches the global observers with the same
    // context the after-rules saw.
    m_server->ruleRegistry()->notifyObservers(f_event, l_ctx);
}

std::optional<QString> ClientSession::takeCharacter(int f_char_id, bool f_announce_done)
{
    akashi::Area *l_area = m_server->areaById(areaId());

    // Protocol hygiene: an id past the roster refuses silently, as always.
    if (f_char_id >= m_server->characterCount()) {
        return QString();
    }

    const int l_from_char_id = m_server->characterId(character());

    // Switching to spectator is a release, not a claim, so no before-rule
    // fires for it. The doors that force the character select screen send
    // their DONE even when the curse gate holds the release, exactly as
    // their old manual sends did.
    if (f_char_id < 0) {
        std::optional<QString> l_refused;
        if (isCharCursed() && !charcurse_list.contains(f_char_id)) {
            l_refused = QString();
        }
        else {
            l_area->changeCharacter(l_from_char_id, -1);
            setCharacter("");
            player()->char_id = f_char_id;
            setSpectator(true);
            // The pair request was made as the released character.
            setPairingWith(-1);
        }
        if (f_announce_done) {
            sendPacket("DONE");
        }
        return l_refused;
    }

    // Person gate: the charcurse sanction pins the player to its list.
    if (isCharCursed() && !charcurse_list.contains(f_char_id)) {
        return QString();
    }

    if (auto l_blocked = checkBeforeRule(akashi::AreaEvents::CharacterChanged,
                                         {{QStringLiteral("character_id"), f_char_id},
                                          {QStringLiteral("character"), m_server->characterById(f_char_id)},
                                          {QStringLiteral("from_character_id"), l_from_char_id}})) {
        return l_blocked;
    }

    // The mechanism: per-area uniqueness refuses a genuinely taken
    // character even for rule-bypassing moderators, silently.
    if (!l_area->changeCharacter(l_from_char_id, f_char_id)) {
        return QString();
    }

    const QString l_char_selected = m_server->characterById(f_char_id);
    setCharacter(l_char_selected);
    player()->char_id = f_char_id;
    // The new character starts on the default side; the commit site
    // refreshes the evidence list when that is an actual side change.
    // Contract: commits announce as they land, so the LE refresh precedes
    // the PV confirmation below - both are idempotent client updates.
    updatePosition(QString());
    // The pair request was made as the previous character.
    setPairingWith(-1);
    m_server->updateCharsTaken(l_area);
    sendPacket("PV", {QString::number(clientId()), "CID", QString::number(f_char_id)});
    runAfterRule(akashi::AreaEvents::CharacterChanged, {{QStringLiteral("character"), l_char_selected}, {QStringLiteral("character_id"), f_char_id}, {QStringLiteral("from_character_id"), l_from_char_id}});
    return std::nullopt;
}

void ClientSession::changePosition(QString f_new_pos)
{
    // The one commit site sanitizes and refreshes the evidence list, and
    // no-ops entirely when the position is unchanged. This door answers
    // either way, echoing what was actually stored.
    updatePosition(f_new_pos);
    sendServerMessage("Position changed to " + player()->pos + ".");
    sendPacket("SP", {player()->pos});
}

void ClientSession::handleCommand(QString command, int argc, QStringList argv)
{
    command = command.toLower();

    // New registry path: if the command is registered, run through the new dispatch.
    akashi::CommandRegistry *l_registry = m_server->commandRegistry();
    if (l_registry && l_registry->contains(command)) {
        if (auto l_spec = l_registry->spec(command)) {
            // A variant command carries its gates per argument shape; the
            // matched form decides which permissions apply and what runs.
            const akashi::CommandVariant *l_variant = l_spec->match(argc);
            if (!l_spec->variants.isEmpty() && !l_variant) {
                sendServerMessage("Invalid command syntax.");
                if (!l_spec->usage.isEmpty()) {
                    sendServerMessage("The expected syntax for this command is: \n" + l_spec->usage);
                }
                return;
            }

            // Permission check: any-of the listed permissions must pass.
            // The same gate CommandRegistry::canUse answers /commands with.
            const QStringList l_permissions = l_variant ? l_variant->permissions : l_spec->permissions;
            if (!akashi::CommandRegistry::passesAnyOf(l_permissions, [this](const QString &f_permission) { return canPerform(f_permission); })) {
                sendServerMessage("You do not have permission to use that command.");
                return;
            }

            if (!l_variant && argc < l_spec->min_args) {
                sendServerMessage("Invalid command syntax.");
                if (!l_spec->usage.isEmpty()) {
                    sendServerMessage("The expected syntax for this command is: \n" + l_spec->usage);
                }
                return;
            }

            akashi::CommandContext l_context(this, m_server, argv);
            if (l_variant) {
                l_variant->handler(l_context);
            }
            else {
                l_registry->handler(command)(l_context);
            }
            return;
        }
    }

    sendServerMessage("Invalid command.");
}

void ClientSession::sendPacket(const akashi::Packet &packet)
{
    write(packet);
}

void ClientSession::sendPacket(QString header, QStringList contents)
{
    sendPacket(akashi::Packet(header, contents));
}

void ClientSession::sendPacket(QString header)
{
    sendPacket(akashi::Packet(header));
}

void ClientSession::calculateIpid()
{
    // TODO: add support for longer ipids?
    // This reduces the (fairly high) chance of
    // birthday paradox issues arising. However,
    // typing more than 8 characters might be a
    // bit cumbersome.

    QCryptographicHash hash(QCryptographicHash::Md5); // Don't need security, just hashing for uniqueness

    hash.addData(remote_ip.toString().toUtf8());

    session_ipid = hash.result().toHex().right(8); // Use the last 8 characters (4 bytes)
}

void ClientSession::sendServerMessage(const QString &message)
{
    sendPacket("CT", {m_server->serverNickname(), message, "1"});
}

void ClientSession::sendServerMessageArea(QString message)
{
    m_server->broadcast(akashi::Packet("CT", {m_server->serverNickname(), message, "1"}), areaId());
}

void ClientSession::sendServerBroadcast(QString message)
{
    m_server->broadcast(akashi::Packet("CT", {m_server->serverNickname(), message, "1"}));
}

QString ClientSession::ipid() const
{
    return session_ipid;
}

ClientSession::SessionStage ClientSession::stage() const
{
    return m_stage;
}

void ClientSession::advanceStage(SessionStage f_stage)
{
    if (f_stage > m_stage) {
        m_stage = f_stage;
    }
}

bool ClientSession::isJoined() const
{
    return m_stage >= SessionStage::Joined;
}

bool ClientSession::isAuthenticated() const
{
    return authenticated;
}

ServerContext *ClientSession::server()
{
    return m_server;
}

int ClientSession::clientId() const
{
    return id;
}

QString ClientSession::name() const
{
    return player()->oocName();
}

void ClientSession::setName(const QString &f_name)
{
    player()->setOocName(f_name);
}

int ClientSession::areaId() const
{
    return player()->areaId();
}

void ClientSession::setAreaId(const int f_area_id)
{
    player()->setAreaId(f_area_id);
}

QString ClientSession::character() const
{
    return player()->character();
}

void ClientSession::setCharacter(const QString &f_character)
{
    player()->setCharacter(f_character);
}

QString ClientSession::characterName() const
{
    return player()->showname();
}

void ClientSession::setCharacterName(const QString &f_showname)
{
    player()->setShowname(f_showname);
}

void ClientSession::setSpectator(bool f_spectator)
{
    player()->spectator = f_spectator;
}

bool ClientSession::isSpectator() const
{
    return player()->spectator;
}

void ClientSession::onAfkTimeout()
{
    if (!afk) {
        sendServerMessage("You are now AFK.");
        setCharacterName(characterName() + " [AFK]");
    }
    afk = true;
}

// How long a connection may sit unidentified before it is dropped. A stock
// client identifies within its first second; anything silent this long is a
// dead or hostile connection squatting a player slot.
static const int IDENTIFICATION_TIMEOUT_MS = 60000;

ClientSession::ClientSession(ServerContext *f_server, akashi::ITransport *f_transport, int f_id, QObject *parent) :
    QObject(parent),
    id(f_id),
    m_server(f_server)
{
    bindTransport(f_transport);
    active_player = addPlayer(f_id, 1);

    afk_timer = new QTimer(this);
    afk_timer->setSingleShot(true);
    connect(afk_timer, &QTimer::timeout, this, &ClientSession::onAfkTimeout);

    reconnect_timer = new QTimer(this);
    reconnect_timer->setSingleShot(true);
    connect(reconnect_timer, &QTimer::timeout, this, &ClientSession::reconnectTimedOut);

    // The session handles the packets its own transport forwards.
    connect(this, &ClientSession::packetReceived, this, &ClientSession::handlePacket);

    QTimer::singleShot(IDENTIFICATION_TIMEOUT_MS, this, [this] {
        if (m_stage < SessionStage::Identified) {
            transport->close();
        }
    });

    if (m_server) {
        packets = m_server->packets();
        if (packets) {
            // Picked again with the real profile once the client identifies.
            codecs = packets->codecs().resolve(m_profile);
        }
    }
}

ClientSession::~ClientSession()
{
    leave();
}

void ClientSession::closeConnection()
{
    transport->close();
}

QString ClientSession::hwid() const
{
    return session_hwid;
}

const akashi::ClientProfile &ClientSession::profile() const
{
    return m_profile;
}

bool ClientSession::isIdentified() const
{
    return m_stage >= SessionStage::Identified;
}

void ClientSession::setHwid(const QString &f_hwid)
{
    session_hwid = f_hwid;
}

void ClientSession::identify(const akashi::ClientProfile &f_profile)
{
    // ID only learns arch and version; the features announced at connect
    // time outlive it.
    const QSet<QString> l_announced = m_profile.features;
    m_profile = f_profile;
    m_profile.features.unite(l_announced);
    advanceStage(SessionStage::Identified);
    if (packets) {
        codecs = packets->codecs().resolve(m_profile);
    }
}

void ClientSession::markJoined()
{
    advanceStage(SessionStage::Joined);
}

void ClientSession::finishJoin()
{
    Q_EMIT joined();
    m_server->areaById(areaId())->addClient(-1, clientId());
    for (akashi::PlayerState *l_player : players) {
        m_server->playerStateObserver()->registerPlayer(l_player);
    }
}

void ClientSession::logConnectionAttempt()
{
    m_server->logService()->log({.type = log_type::Connect,
                                 .ipid = session_ipid,
                                 .target_ipid = remote_ip.toString(),
                                 .hwid = session_hwid});
}

std::optional<akashi::BanRecord> ClientSession::hardwareBan() const
{
    const auto l_ban = m_server->databaseManager()->isHDIDBanned(session_hwid);
    if (!l_ban.first) {
        return std::nullopt;
    }

    akashi::BanRecord l_record;
    l_record.id = l_ban.second.id;
    l_record.reason = l_ban.second.reason;
    l_record.permanent = l_ban.second.duration == -2;
    if (!l_record.permanent) {
        l_record.end = QDateTime::fromSecsSinceEpoch(l_ban.second.time).addSecs(l_ban.second.duration);
    }
    return l_record;
}

QString ClientSession::serverNickname() const { return m_server->serverNickname(); }
int ClientSession::maxMessageLength() const { return m_server->serverSettings()->maximum_characters(); }
bool ClientSession::webaoEnabled() const { return m_server->serverSettings()->webao_enable(); }
int ClientSession::maxPlayerCount() const { return m_server->serverSettings()->max_players(); }
QString ClientSession::serverDescription() const { return m_server->serverSettings()->server_description(); }
QUrl ClientSession::assetUrl() const { return m_server->assetUrl(); }
QString ClientSession::motd() const { return m_server->serverSettings()->motd(); }

int ClientSession::playerCount() const
{
    return m_server->playerCount();
}

QStringList ClientSession::characters() const
{
    return m_server->characters();
}

QStringList ClientSession::musicList() const
{
    return m_server->musicList();
}

akashi::TimerSnapshot ClientSession::globalTimer() const
{
    return akashi::TimerSnapshot{m_server->timer->isActive(), QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(m_server->timer->remainingTime()))};
}

void ClientSession::announceCharsTaken()
{
    m_server->updateCharsTaken(m_server->areaById(areaId()));
}

void ClientSession::sendEvidenceList()
{
    sendEvidenceList(m_server->areaById(areaId()));
}

void ClientSession::sendFullArup()
{
    m_server->arupBroadcaster()->sendFullArup(clientId(), m_server->floorIdForArea(areaId()));
}

void ClientSession::broadcastPlayerCount()
{
    m_server->arupBroadcaster()->broadcastNow(akashi::ArupBroadcaster::Type::PlayerCount);
}

bool ClientSession::selectCharacter(int f_char_id)
{
    // A host-attached rule speaks its reason; the mechanism's refusal is
    // silent and leaves the client on its character select screen.
    if (auto l_blocked = takeCharacter(f_char_id); l_blocked && !l_blocked->isEmpty()) {
        sendServerMessage(*l_blocked);
    }
    if (player()->char_id > SPECTATOR_ID) {
        setSpectator(false);
    }
    return player()->char_id == f_char_id;
}

bool ClientSession::canUseOocChat() const
{
    return !hasSanction(akashi::sanction::ooc_muted);
}

QString ClientSession::oocName() const
{
    return name();
}

void ClientSession::setOocName(const QString &f_name)
{
    setName(f_name);
}

bool ClientSession::isInLoginPrompt() const
{
    return logging_in;
}

void ClientSession::attemptLogin(const QString &f_message)
{
    loginAttempt(f_message);
}

void ClientSession::runCommand(const QString &f_command, const QStringList &f_arguments)
{
    QStringList l_logged_args = f_arguments;
    if (auto l_spec = m_server->commandRegistry()->spec(f_command); l_spec && l_spec->sensitive_args_from >= 0) {
        for (int i = l_spec->sensitive_args_from; i < l_logged_args.size(); ++i) {
            l_logged_args[i] = QStringLiteral("***");
        }
    }
    m_server->logService()->log({.type = log_type::CMD,
                                 .area = m_server->areaById(areaId())->name(),
                                 .char_name = character() + " " + characterName(),
                                 .ooc_name = name(),
                                 .ipid = session_ipid,
                                 .message = f_command,
                                 .args = l_logged_args.join(QStringLiteral(" "))});

    handleCommand(f_command, f_arguments.size(), f_arguments);
}

void ClientSession::broadcastOoc(const QString &f_message)
{
    m_server->broadcast(akashi::Packet("CT", {name(), f_message, "0"}), areaId());
    m_server->logService()->log({.type = log_type::OOC,
                                 .area = m_server->areaById(areaId())->name(),
                                 .char_name = character() + " " + characterName(),
                                 .ooc_name = name(),
                                 .ipid = session_ipid,
                                 .client_id = QString::number(clientId()),
                                 .message = f_message});
}

bool ClientSession::canModifyEvidence()
{
    return canModifyEvidence(m_server->areaById(areaId()));
}

int ClientSession::evidenceCount() const
{
    return m_server->areaById(areaId())->evidence().size();
}

void ClientSession::deleteEvidence(int f_index)
{
    akashi::Area *l_area = m_server->areaById(areaId());
    // The client counts the list it was shown, which in hidden mode is not
    // the whole record - every incoming evidence number must be translated
    // through this viewer's own list, the way tsu3 calculated the positions
    // by hand. A protocol update replaces this with stable item ids.
    const int l_real_index = l_area->evidenceIndexByVisibleIndex(f_index + 1, player()->pos, canPerform(permission::gamemaster));
    l_area->deleteEvidence(l_real_index);
}

void ClientSession::replaceEvidence(int f_index, const QString &f_name, const QString &f_description, const QString &f_image)
{
    akashi::Area *l_area = m_server->areaById(areaId());
    // Same viewer translation as deleteEvidence; an unknown number maps to
    // -1 and the store refuses it, so a stale list cannot hit the wrong item.
    const int l_real_index = l_area->evidenceIndexByVisibleIndex(f_index + 1, player()->pos, canPerform(permission::gamemaster));
    akashi::Evidence l_evidence;
    l_evidence.name = f_name;
    // A hidden-CM area writes the <owner=all> tag into untagged items.
    l_evidence.description = l_area->taggedEvidenceDescription(f_description);
    l_evidence.image = f_image;
    l_area->replaceEvidence(l_real_index, l_evidence);
}

void ClientSession::setCasingPreferences(const QList<bool> &f_preferences)
{
    casing_preferences = f_preferences;
}

bool ClientSession::canUseIcChat() const
{
    return !hasSanction(akashi::sanction::muted);
}

int ClientSession::characterId() const
{
    return player()->char_id;
}

bool ClientSession::isFirstPerson() const
{
    return player()->first_person;
}

void ClientSession::setIniswap(const QString &f_character)
{
    player()->iniswap = f_character;
}

void ClientSession::setEmote(const QString &f_emote)
{
    player()->emote = f_emote;
}

void ClientSession::setOffset(const QString &f_offset)
{
    player()->offset = f_offset;
}

void ClientSession::setFlipping(const QString &f_flipping)
{
    player()->flipping = f_flipping;
}

QString ClientSession::iniswap() const
{
    return player()->iniswap;
}

QString ClientSession::emote() const
{
    return player()->emote;
}

QString ClientSession::offset() const
{
    return player()->offset;
}

QString ClientSession::flipping() const
{
    return player()->flipping;
}

QString ClientSession::pos() const
{
    return player()->pos;
}

int ClientSession::pairingWith() const
{
    return player()->pairing_with;
}

void ClientSession::setPairingWith(int f_char_id)
{
    // pairing_with records the last successfully sent pair request: a
    // refused message never flips it, so a partner's mutual-pair check
    // keeps matching against the sender's last valid request. It is
    // cleared when the sender changes character or area, where the
    // request loses its meaning.
    player()->pairing_with = f_char_id;
}

QString ClientSession::lastIcMessage() const
{
    return player()->last_message;
}

void ClientSession::setLastIcMessage(const QString &f_message)
{
    player()->last_message = f_message;
}

void ClientSession::updatePosition(const QString &f_position)
{
    // Sanitize before comparing, so an input differing only by a traversal
    // prefix from the current side commits nothing and refreshes nothing.
    const QString l_position = akashi::sanitizePosition(f_position);
    if (player()->pos != l_position) {
        player()->pos = l_position;
        updateEvidenceList(m_server->areaById(areaId()));
    }
}

std::optional<QString> ClientSession::applyTextFilters(const QString &f_text, akashi::TextChannel f_channel) const
{
    // The area's medieval mode is no personal sanction: the apply_medieval
    // transform rule reads that knob.
    return m_server->textFilterRegistry()->apply(f_text, sanctions, f_channel);
}

bool ClientSession::isAfk() const
{
    return afk;
}

void ClientSession::setAfk(bool f_afk)
{
    afk = f_afk;
}

bool ClientSession::isPmMuted() const
{
    return pm_muted;
}

void ClientSession::setPmMuted(bool f_pm_muted)
{
    pm_muted = f_pm_muted;
}

bool ClientSession::isAdvertEnabled() const
{
    return advert_enabled;
}

void ClientSession::setAdvertEnabled(bool f_advert_enabled)
{
    advert_enabled = f_advert_enabled;
}

bool ClientSession::isCharCursed() const
{
    return hasSanction(akashi::sanction::charcurse);
}

void ClientSession::setCharCursed(bool f_char_cursed)
{
    setSanction(akashi::sanction::charcurse, f_char_cursed);
}

bool ClientSession::isTestimonySaving() const
{
    return testimony_saving;
}

void ClientSession::setTestimonySaving(bool f_testimony_saving)
{
    testimony_saving = f_testimony_saving;
}

QHostAddress ClientSession::remoteIp() const
{
    return remote_ip;
}

QString ClientSession::moderatorName() const
{
    return moderator_name;
}

void ClientSession::setModeratorName(const QString &f_name)
{
    moderator_name = f_name;
}

QString ClientSession::aclRoleId() const
{
    return acl_role_id;
}

void ClientSession::setAclRoleId(const QString &f_role_id)
{
    acl_role_id = f_role_id;
}

void ClientSession::setAuthenticated(bool f_state)
{
    authenticated = f_state;
}

void ClientSession::setInLoginPrompt(bool f_in_login_prompt)
{
    logging_in = f_in_login_prompt;
}

void ClientSession::setFirstPerson(bool f_first_person)
{
    player()->first_person = f_first_person;
}

bool ClientSession::isGlobalEnabled() const
{
    return global_enabled;
}

void ClientSession::setGlobalEnabled(bool f_global_enabled)
{
    global_enabled = f_global_enabled;
}

QList<bool> ClientSession::casingPreferences() const
{
    return casing_preferences;
}

QList<int> ClientSession::charCurseList() const
{
    return charcurse_list;
}

void ClientSession::addCharCurse(int f_char_id)
{
    charcurse_list.append(f_char_id);
}

void ClientSession::clearCharCurse()
{
    charcurse_list.clear();
}

void ClientSession::closeSocket()
{
    transport->close();
}

bool ClientSession::isIcMessageAllowed() const
{
    return m_server->areaById(areaId())->isMessageAllowed() && m_server->isMessageAllowed();
}

bool ClientSession::canActInArea()
{
    akashi::Area *l_area = m_server->areaById(areaId());
    return !(l_area->lockState() == akashi::Area::LockState::Spectatable && !l_area->invited().contains(clientId()) && !canPerform(permission::bypass_locks));
}

bool ClientSession::isImmediateForced() const
{
    return m_server->areaById(areaId())->forceImmediate();
}

QString ClientSession::areaSide() const
{
    return m_server->areaById(areaId())->side();
}

QStringList ClientSession::lastAreaMessage() const
{
    return m_server->areaById(areaId())->lastICMessage();
}

akashi::PairInfo ClientSession::resolvePair(int f_pair_id)
{
    // A pure query over committed state: the sender's own request commits
    // through setPairingWith with the rest of the message.
    akashi::PairInfo l_pair;
    akashi::Area *l_area = m_server->areaById(areaId());
    const QList<int> l_joined = l_area->players();
    for (int l_client_id : l_joined) {
        ClientSession *l_client = m_server->clientById(l_client_id);
        if (l_client == nullptr) {
            continue;
        }
        if (l_client->pairingWith() == player()->char_id && f_pair_id != player()->char_id && l_client->characterId() == f_pair_id && l_client->pos() == player()->pos) {
            l_pair.name = l_client->iniswap();
            l_pair.emote = l_client->emote();
            l_pair.offset = l_client->offset();
            l_pair.flip = l_client->flipping();
            l_pair.paired = true;
        }
    }
    return l_pair;
}

QStringList ClientSession::applyTestimony(const QStringList &f_fields)
{
    QStringList l_args = f_fields;
    akashi::TestimonyRecorder *l_recorder = m_server->areaById(areaId())->testimonyRecorder();
    using State = akashi::TestimonyRecorder::State;

    QString client_name = name();
    if (client_name == "") {
        client_name = character(); // fallback in case of empty ooc name
    }

    const State l_state = l_recorder->state();
    if ((l_state == State::Recording || l_state == State::Add) && !l_args[4].isEmpty()) {
        // -1 indicates title
        if (l_recorder->statementIndex() == -1) {
            l_args[4] = "~~-- " + l_args[4] + " --";
            l_args[14] = "3";
            m_server->broadcast(akashi::Packet("RT", {"testimony1", "0"}), areaId());
        }
        addStatement(l_args);
    }
    else if (l_state == State::Update) {
        l_args = updateStatement(l_args);
    }
    else if (l_state == State::Playback) {
        std::optional<QPair<akashi::Statement, akashi::TestimonyRecorder::Playback>> l_jump;
        bool l_navigating = false;

        static const QRegularExpression s_jump("(?<arrow>>|<)(?<int>\\d+)");
        const QRegularExpressionMatch match = s_jump.match(l_args[4]);

        if (l_args[4] == ">") {
            l_navigating = true;
            l_jump = l_recorder->jumpTo(l_recorder->statementIndex() + 1);
            if (l_jump) {
                sendServerMessageArea(client_name + " moved to the next statement.");
                if (l_jump->second == akashi::TestimonyRecorder::Playback::Looped) {
                    sendServerMessageArea("Last statement reached. Looping to first statement.");
                }
            }
        }
        else if (l_args[4] == "<") {
            l_navigating = true;
            l_jump = l_recorder->jumpTo(l_recorder->statementIndex() - 1);
            if (l_jump) {
                sendServerMessageArea(client_name + " moved to the previous statement.");
                if (l_jump->second == akashi::TestimonyRecorder::Playback::StayedAtFirst) {
                    sendServerMessage("First statement reached.");
                }
            }
        }
        else if (l_args[4] == "=") {
            l_navigating = true;
            l_jump = l_recorder->jumpTo(l_recorder->statementIndex());
            if (l_jump) {
                sendServerMessageArea(client_name + " repeated the current statement.");
            }
        }
        else if (match.hasMatch()) {
            l_navigating = true;
            const int jump_idx = match.captured("int").toInt();
            l_jump = l_recorder->jumpTo(jump_idx);
            if (l_jump) {
                sendServerMessageArea(client_name + " jumped to statement number " + QString::number(jump_idx) + ".");
                if (l_jump->second == akashi::TestimonyRecorder::Playback::Looped) {
                    sendServerMessageArea("Last statement reached. Looping to first statement.");
                }
                else if (l_jump->second == akashi::TestimonyRecorder::Playback::StayedAtFirst) {
                    sendServerMessage("First statement reached.");
                }
            }
        }

        if (l_jump) {
            // Replayed statements belong to nobody, so no client mistakes
            // them for its own message and clears the player's input panel.
            l_args = l_jump->first.playbackFields();
            // The jumper follows the statement to its position through the
            // one commit site, so the side-dependent evidence list refreshes.
            updatePosition(l_jump->first.side());
        }
        else if (l_navigating) {
            // The old code crashed here: playback with nothing recorded.
            sendServerMessage("There is no testimony to play back.");
        }
    }

    return l_args;
}

void ClientSession::addStatement(QStringList packet)
{
    if (checkTestimonySymbols(packet[4])) {
        return;
    }
    akashi::TestimonyRecorder *l_recorder = m_server->areaById(areaId())->testimonyRecorder();
    using State = akashi::TestimonyRecorder::State;
    akashi::Statement l_statement(packet);

    if (l_recorder->state() == State::Recording) {
        if (l_recorder->statementIndex() <= m_server->serverSettings()->maximum_statements()) {
            l_statement.setTextColor(l_recorder->statementIndex() == -1 ? "3" : "1");
            l_recorder->record(l_statement);
        }
        else {
            sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
        }
    }
    else if (l_recorder->state() == State::Add) {
        // Behind the current statement; never before the title, which the
        // old code got wrong and displaced the title with.
        l_statement.setTextColor("1");
        l_recorder->insert(qMax(l_recorder->statementIndex(), 0) + 1, l_statement);
        l_recorder->setState(State::Playback);
    }
}

QStringList ClientSession::updateStatement(QStringList packet)
{
    if (checkTestimonySymbols(packet[4])) {
        return packet;
    }
    akashi::TestimonyRecorder *l_recorder = m_server->areaById(areaId())->testimonyRecorder();
    l_recorder->setState(akashi::TestimonyRecorder::State::Playback);
    const int l_index = l_recorder->statementIndex();
    if (l_index <= 0 || !l_recorder->statementAt(l_index)) {
        sendServerMessage("Unable to update an empty statement. Please use /addtestimony.");
        return packet;
    }
    akashi::Statement l_statement(packet);
    l_statement.setTextColor("1");
    l_recorder->replace(l_index, l_statement);
    sendServerMessage("Updated current statement.");
    return l_statement.icFields();
}

bool ClientSession::checkTestimonySymbols(const QString &message)
{
    if (message.contains('>') || message.contains('<')) {
        sendServerMessage("Unable to add statements containing '>' or '<'.");
        return true;
    }
    return false;
}

void ClientSession::broadcastIc(const QStringList &f_fields, int f_evidence_index)
{
    QStringList l_fields = f_fields;
    if (player()->pos != "") {
        l_fields[5] = player()->pos;
    }
    akashi::Area *l_area = m_server->areaById(areaId());

    // Presenting evidence in a hidden-CM area reveals it to everyone, and each
    // client gets the index as it appears in its own filtered list.
    int l_real_index = -1;
    bool l_evidence_presented = false;
    if (f_evidence_index > 0 && l_area->evidenceAccess() == akashi::EvidenceStore::Access::HiddenCm) {
        l_real_index = l_area->evidenceIndexByVisibleIndex(f_evidence_index, player()->pos, canPerform(permission::gamemaster));
        if (l_real_index >= 0) {
            l_area->setEvidenceOwnerToAll(l_real_index);
            sendEvidenceList(l_area);
            l_evidence_presented = true;
        }
    }

    // The server owns the outgoing encode; the reveal above only adds
    // the per-viewer index remap.
    std::function<QStringList(ClientSession *)> l_viewer_fields;
    if (l_evidence_presented) {
        l_viewer_fields = [l_fields, l_area, l_real_index](ClientSession *f_client) {
            QStringList l_client_fields = l_fields;
            l_client_fields[11] = QString::number(l_area->visibleIndexByEvidenceIndex(l_real_index, f_client->pos(), f_client->canPerform(permission::gamemaster)));
            return l_client_fields;
        };
    }
    m_server->broadcastIc(l_fields, areaId(), l_viewer_fields);

    m_server->logService()->log({.type = log_type::IC,
                                 .area = l_area->name(),
                                 .char_name = character() + " " + characterName(),
                                 .ooc_name = name(),
                                 .ipid = session_ipid,
                                 .client_id = QString::number(clientId()),
                                 .message = player()->last_message});
    l_area->updateLastICMessage(l_fields);

    l_area->startMessageFloodguard(m_server->serverSettings()->message_floodguard());
    m_server->startMessageFloodguard(m_server->serverSettings()->global_message_floodguard());
}

bool ClientSession::hasSong(const QString &f_name) const
{
    return m_server->areaById(areaId())->jukebox()->hasSong(f_name);
}

bool ClientSession::isDjBlocked() const
{
    return hasSanction(akashi::sanction::dj_blocked);
}

std::optional<QString> ClientSession::playMusic(const QString &f_song, akashi::MusicSource f_source, const QString &f_char_id, const QString &f_effects)
{
    const QString l_source = akashi::musicSourceName(f_source);
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::MusicChanged, {{QStringLiteral("song"), f_song}, {QStringLiteral("source"), l_source}})) {
        return l_rule_block;
    }

    QString l_song = f_song;
    // A category on the list has no file extension; picking one stops the
    // music.
    if (f_source == akashi::MusicSource::List && !l_song.contains(QStringLiteral("."))) {
        l_song = ao2::STOP_MUSIC;
    }

    akashi::Area *l_area = m_server->areaById(areaId());

    // The jukebox intercepts list picks. The queue's answer is feedback to
    // the requester, not a refusal.
    if (f_source == akashi::MusicSource::List && l_area->isJukeboxEnabled()) {
        sendServerMessage(l_area->jukebox()->queueSong(clientId(), l_song));
        return std::nullopt;
    }

    // A list name may be an alias for the real file.
    if (f_source == akashi::MusicSource::List && l_song != ao2::STOP_MUSIC) {
        if (const auto l_info = l_area->jukebox()->songInfo(l_song)) {
            l_song = l_info->real_name;
        }
    }

    m_server->logService()->log({.type = log_type::Music,
                                 .area = l_area->name(),
                                 .char_name = character() + " " + characterName(),
                                 .ooc_name = name(),
                                 .ipid = session_ipid,
                                 .message = l_song});

    // An empty showname would show as "played by ." in /currentmusic.
    l_area->changeMusic(characterName().isEmpty() ? character() : characterName(), l_song);

    // Both doors broadcast the same MC shape: the list door echoes the
    // client's claimed character id and effects, /play resolves the id
    // itself and sends no effects field.
    QStringList l_fields = {l_song, f_char_id, characterName(), QStringLiteral("1"), QStringLiteral("0")};
    if (f_source == akashi::MusicSource::List) {
        l_fields.append(f_effects);
    }
    else {
        l_fields[1] = QString::number(m_server->characterId(character()));
    }
    broadcastArea(akashi::Packet(QStringLiteral("MC"), l_fields));

    runAfterRule(akashi::AreaEvents::MusicChanged, {{QStringLiteral("song"), l_song}, {QStringLiteral("source"), l_source}});
    return std::nullopt;
}

std::optional<QString> ClientSession::playAmbience(const QString &f_song, akashi::MusicSource f_source)
{
    const QString l_source = akashi::musicSourceName(f_source);
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::AmbienceChanged, {{QStringLiteral("song"), f_song}, {QStringLiteral("source"), l_source}})) {
        return l_rule_block;
    }

    m_server->areaById(areaId())->changeAmbience(f_song);
    broadcastArea(akashi::Packet(QStringLiteral("MC"), {f_song, QStringLiteral("-1"), characterName(), QStringLiteral("1"), QStringLiteral("1")}));

    runAfterRule(akashi::AreaEvents::AmbienceChanged, {{QStringLiteral("song"), f_song}, {QStringLiteral("source"), l_source}});
    return std::nullopt;
}

std::optional<QString> ClientSession::changeBackground(const QString &f_background)
{
    const QVariantMap l_payload = {{QStringLiteral("background"), f_background}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::BackgroundChanged, l_payload)) {
        return l_rule_block;
    }

    akashi::Area *l_area = m_server->areaById(areaId());
    if (!m_server->backgrounds().contains(f_background, Qt::CaseInsensitive) && !l_area->ignoreBgList()) {
        return QStringLiteral("Invalid background name.");
    }

    l_area->setBackground(f_background);
    broadcastArea(akashi::Packet(QStringLiteral("BN"), {f_background, l_area->side()}));

    // The background brings its own ambience; the ambience verb owns that
    // change, so its rules see the source "background" and exactly one MC
    // goes out. No configured song means silence.
    const QString l_ambience = m_server->configStore()->settings("ambience")->value(f_background + "/ambience").toString();
    if (auto l_refusal = playAmbience(l_ambience.isEmpty() ? QString(ao2::STOP_MUSIC) : l_ambience, akashi::MusicSource::Background)) {
        sendServerMessage(*l_refusal);
    }

    runAfterRule(akashi::AreaEvents::BackgroundChanged, l_payload);
    sendServerMessageArea(character() + " changed the background to " + f_background);
    return std::nullopt;
}

std::optional<QString> ClientSession::changeBackgroundSide(const QString &f_side)
{
    const QVariantMap l_payload = {{QStringLiteral("side"), f_side}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::BackgroundChanged, l_payload)) {
        return l_rule_block;
    }

    akashi::Area *l_area = m_server->areaById(areaId());
    l_area->setSide(f_side);
    broadcastArea(akashi::Packet(QStringLiteral("BN"), {l_area->background(), f_side}));

    runAfterRule(akashi::AreaEvents::BackgroundChanged, l_payload);
    return std::nullopt;
}

std::optional<QString> ClientSession::changeAreaStatus(const QString &f_name)
{
    const QString l_name = f_name.toLower();
    const std::optional<QString> l_status = akashi::Area::canonicalStatus(l_name);
    if (!l_status) {
        return QStringLiteral("That does not look like a valid status. Valid statuses are idle, rp, casing, lfp, looking-for-players, recess, gaming");
    }

    const QVariantMap l_payload = {{QStringLiteral("status"), l_name}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::StatusChanged, l_payload)) {
        return l_rule_block;
    }

    m_server->areaById(areaId())->setStatus(*l_status);
    sendServerMessageArea(character() + " changed status to " + l_name.toUpper());

    runAfterRule(akashi::AreaEvents::StatusChanged, l_payload);
    return std::nullopt;
}

std::optional<QString> ClientSession::changeDocument(const QString &f_text)
{
    const QVariantMap l_payload = {{QStringLiteral("document"), f_text}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::DocChanged, l_payload)) {
        return l_rule_block;
    }

    // The classic protocol has no document packet, so the change only shows
    // through /doc; the verb still centralizes the rules and the notice.
    akashi::Area *l_area = m_server->areaById(areaId());
    if (f_text.isEmpty()) {
        l_area->changeDoc(QStringLiteral("No document."));
        sendServerMessageArea(name() + " cleared the document.");
    }
    else {
        l_area->changeDoc(f_text);
        sendServerMessageArea(name() + " changed the document.");
    }

    runAfterRule(akashi::AreaEvents::DocChanged, l_payload);
    return std::nullopt;
}

std::optional<QString> ClientSession::setAreaLock(akashi::Area::LockState f_state)
{
    QString l_state_name;
    switch (f_state) {
    case akashi::Area::LockState::Locked:
        l_state_name = QStringLiteral("locked");
        break;
    case akashi::Area::LockState::Spectatable:
        l_state_name = QStringLiteral("spectatable");
        break;
    case akashi::Area::LockState::Free:
        l_state_name = QStringLiteral("free");
        break;
    }

    const QVariantMap l_payload = {{QStringLiteral("state"), l_state_name}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::LockChanged, l_payload)) {
        return l_rule_block;
    }

    akashi::Area *l_area = m_server->areaById(areaId());
    l_area->setLockState(f_state);

    // Closing the door invites everyone already inside.
    if (f_state != akashi::Area::LockState::Free) {
        const QVector<ClientSession *> l_clients = m_server->clients();
        for (ClientSession *l_client : l_clients) {
            if (l_client->areaId() == areaId() && l_client->isJoined()) {
                l_area->invite(l_client->clientId());
            }
        }
    }

    runAfterRule(akashi::AreaEvents::LockChanged, l_payload);
    return std::nullopt;
}

std::optional<QString> ClientSession::addAreaOwner(int f_target)
{
    const QVariantMap l_payload = {{QStringLiteral("owner"), f_target}, {QStringLiteral("added"), true}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::OwnerChanged, l_payload)) {
        return l_rule_block;
    }

    m_server->areaById(areaId())->addOwner(f_target);

    runAfterRule(akashi::AreaEvents::OwnerChanged, l_payload);
    return std::nullopt;
}

std::optional<QString> ClientSession::removeAreaOwner(int f_target)
{
    const QVariantMap l_payload = {{QStringLiteral("owner"), f_target}, {QStringLiteral("added"), false}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::OwnerChanged, l_payload)) {
        return l_rule_block;
    }

    m_server->areaById(areaId())->removeOwner(f_target);

    runAfterRule(akashi::AreaEvents::OwnerChanged, l_payload);
    return std::nullopt;
}

bool ClientSession::isWtceBlocked() const
{
    return hasSanction(akashi::sanction::wtce_blocked);
}

bool ClientSession::startWtceCooldown()
{
    const qint64 l_now = QDateTime::currentDateTime().toSecsSinceEpoch();
    if (l_now - last_wtce_time <= 5) {
        return false;
    }
    last_wtce_time = l_now;
    return true;
}

std::optional<QString> ClientSession::useWtce(const QString &f_splash, const std::optional<QString> &f_variant)
{
    const QVariantMap l_payload = {{QStringLiteral("kind"), QStringLiteral("wtce")}, {QStringLiteral("splash"), f_splash}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::WtceUsed, l_payload)) {
        return l_rule_block;
    }

    QStringList l_fields = {f_splash};
    if (f_variant) {
        l_fields.append(*f_variant);
    }
    broadcastArea(akashi::Packet(QStringLiteral("RT"), l_fields));
    updateJudgeLog(m_server->areaById(areaId()), this, QStringLiteral("WT/CE"));

    runAfterRule(akashi::AreaEvents::WtceUsed, l_payload);
    return std::nullopt;
}

std::optional<QString> ClientSession::changePenalty(int f_bar, int f_value)
{
    const QVariantMap l_payload = {{QStringLiteral("kind"), QStringLiteral("penalty")}, {QStringLiteral("bar"), f_bar}, {QStringLiteral("value"), f_value}};
    if (auto l_rule_block = checkBeforeRule(akashi::AreaEvents::WtceUsed, l_payload)) {
        return l_rule_block;
    }

    akashi::Area *l_area = m_server->areaById(areaId());
    // An unknown bar changes nothing, but both bars still echo.
    if (f_bar == 1) {
        l_area->changeHP(akashi::Area::Side::DEFENCE, f_value);
    }
    else if (f_bar == 2) {
        l_area->changeHP(akashi::Area::Side::PROSECUTOR, f_value);
    }
    broadcastArea(akashi::Packet(QStringLiteral("HP"), {QStringLiteral("1"), QString::number(l_area->defHP())}));
    broadcastArea(akashi::Packet(QStringLiteral("HP"), {QStringLiteral("2"), QString::number(l_area->proHP())}));
    updateJudgeLog(l_area, this, QStringLiteral("updated the penalties"));

    runAfterRule(akashi::AreaEvents::WtceUsed, l_payload);
    return std::nullopt;
}

void ClientSession::addEvidence(const QString &f_name, const QString &f_description, const QString &f_image)
{
    akashi::Area *l_area = m_server->areaById(areaId());
    akashi::Evidence l_evidence;
    l_evidence.name = f_name;
    // A hidden-CM area writes the <owner=all> tag into untagged items.
    l_evidence.description = l_area->taggedEvidenceDescription(f_description);
    l_evidence.image = f_image;
    l_area->appendEvidence(l_evidence);
    sendEvidenceList(l_area);
}

void ClientSession::broadcastArea(const akashi::Packet &f_packet)
{
    m_server->broadcast(f_packet, areaId());
}

void ClientSession::broadcastCaseAlert(const QList<bool> &f_needs, const akashi::Packet &f_packet)
{
    const QSet<bool> l_needs_set(f_needs.begin(), f_needs.end());
    const QVector<ClientSession *> l_clients = m_server->clients();
    for (ClientSession *l_client : l_clients) {
        const QList<bool> l_preferences = l_client->casingPreferences();
        QSet<bool> l_matches(l_preferences.begin(), l_preferences.end());
        l_matches.intersect(l_needs_set);
        if (!l_matches.isEmpty()) {
            l_client->sendPacket(f_packet);
        }
    }
}

void ClientSession::setCharacterPassword(const QString &f_password)
{
    password = f_password;
}

bool ClientSession::canPerform(const QString &f_permission) const
{
    akashi::PermissionQuery l_query;
    l_query.permission = f_permission;
    l_query.client_id = clientId();
    l_query.area_id = areaId();
    l_query.is_authenticated = isAuthenticated();
    // The blanket simple-auth grant keys off the ACTIVE system, so a
    // plugin system's logins resolve through their roles, never through
    // the simple shortcut. Byte-identical for the two core systems.
    l_query.auth_type = m_server->activeAuthSystemId() == QStringLiteral("password")
                            ? QStringLiteral("simple")
                            : QStringLiteral("advanced");
    l_query.acl_role_id = acl_role_id;
    return m_server->permissionRegistry()->resolve(l_query);
}

QString ClientSession::areaName() const
{
    return m_server->areaById(areaId())->name();
}

std::optional<QString> ClientSession::playerName(int f_client_id) const
{
    ClientSession *l_client = m_server->clientById(f_client_id);
    if (l_client == nullptr) {
        return std::nullopt;
    }
    return l_client->name();
}

void ClientSession::broadcastModerators(const akashi::Packet &f_packet)
{
    const QVector<ClientSession *> l_clients = m_server->clients();
    for (ClientSession *l_client : l_clients) {
        if (l_client->isAuthenticated()) {
            l_client->sendPacket(f_packet);
        }
    }
}

void ClientSession::recordModcall(const QString &f_reason)
{
    const QString l_area_name = m_server->areaById(areaId())->name();
    m_server->logService()->log({.type = log_type::Modcall,
                                 .area = l_area_name,
                                 .char_name = character() + " " + characterName(),
                                 .ooc_name = name(),
                                 .ipid = session_ipid,
                                 .client_id = QString::number(clientId())});
    m_server->flushModcallLog(l_area_name);

    const akashi::ModcallEvent l_event{
        .client_session_id = clientId(),
        .player_state_id = player()->id(),
        .area_id = areaId(),
        .area_name = l_area_name,
        .char_name = character(),
        .ooc_name = name(),
        .ipid = session_ipid,
        .reason = f_reason};
    m_server->publishEvent(akashi::ModcallEvent::id, akashi::eventToMap(l_event), player()->id(), clientId());
}

// The MA packet's kick door: resolves the target and phrases the reply;
// the act itself is the server's kick verb.
void ClientSession::kickPlayer(int f_client_id, const QString &f_reason)
{
    ClientSession *l_target = m_server->clientById(f_client_id);
    if (l_target == nullptr) {
        return;
    }
    QString l_moderator_name = "Moderator";
    if (m_server->authType() == AuthType::ADVANCED) {
        l_moderator_name = moderator_name;
    }

    const QList<ClientSession *> l_clients = m_server->clientsByIpid(l_target->ipid());
    m_server->kickClients(l_clients, l_target->ipid(), f_reason, l_moderator_name);
    sendServerMessage("Kicked " + QString::number(l_clients.size()) + " client(s) with ipid " + l_target->ipid() + " for reason: " + f_reason);
}

// The MA packet's ban door: minutes become seconds and the KB carries the
// bare reason; the act itself is the server's ban verb.
void ClientSession::banPlayer(int f_client_id, int f_duration, const QString &f_reason)
{
    ClientSession *l_target = m_server->clientById(f_client_id);
    if (l_target == nullptr) {
        return;
    }
    QString l_moderator_name = "Moderator";
    if (m_server->authType() == AuthType::ADVANCED) {
        l_moderator_name = moderator_name;
    }

    const long long l_duration_secs = f_duration == -1 ? -2 : static_cast<long long>(f_duration) * 60;
    const auto l_result = m_server->banPlayers(l_target->ipid(), l_duration_secs, f_reason, l_moderator_name, QStringLiteral("permanently"));
    sendServerMessage("Banned " + QString::number(l_result.clients_kicked) + " client(s) with ipid " + l_target->ipid() + " for reason: " + f_reason);
}

void ClientSession::sendEvidenceList(akashi::Area *area) const
{
    const QVector<ClientSession *> l_clients = m_server->clients();
    for (ClientSession *l_client : l_clients) {
        if (l_client->areaId() == areaId())
            l_client->updateEvidenceList(area);
    }
}

void ClientSession::updateEvidenceList(akashi::Area *area)
{
    QStringList l_evidence_list;

    // The store applies the hidden-items rules; the same filter also drives
    // the visible-index translation, so the two can never disagree.
    const QList<akashi::Evidence> l_visible = area->visibleEvidence(canPerform(permission::gamemaster), player()->pos);
    for (const akashi::Evidence &l_item : l_visible) {
        l_evidence_list.append(l_item.toLeField());
    }

    sendPacket(akashi::Packet("LE", l_evidence_list));
}

QString ClientSession::dezalgo(QString p_text)
{
    return akashi::stripZalgo(p_text);
}

bool ClientSession::canModifyEvidence(akashi::Area *area)
{
    switch (area->evidenceAccess()) {
    case akashi::EvidenceStore::Access::FreeForAll:
        return true;
    case akashi::EvidenceStore::Access::Cm:
    case akashi::EvidenceStore::Access::HiddenCm:
        return canPerform(permission::gamemaster);
    case akashi::EvidenceStore::Access::Mod:
        return authenticated;
    default:
        return false;
    }
}

void ClientSession::updateJudgeLog(akashi::Area *area, ClientSession *client, QString action)
{
    QString l_timestamp = QTime::currentTime().toString("hh:mm:ss");
    QString l_uid = QString::number(client->clientId());
    QString l_char_name = client->character();
    QString l_ipid = client->ipid();
    QString l_message = action;
    QString l_logmessage = QString("[%1]: [%2] %3 (%4) %5").arg(l_timestamp, l_uid, l_char_name, l_ipid, l_message);
    area->appendJudgelog(l_logmessage);
}

QString ClientSession::decodeMessage(QString incoming_message)
{
    QString decoded_message = incoming_message.replace("<num>", "#")
                                  .replace("<percent>", "%")
                                  .replace("<dollar>", "$")
                                  .replace("<and>", "&");
    return decoded_message;
}

// The prompt speaks only the two core dialects; /login refuses to open it
// for anything else. The synchronous refusals (wrong modpass, unknown
// user) answer before the exit line, the deferred hash outcome after it -
// exactly the bytes the prompt has always sent.
void ClientSession::loginAttempt(QString message)
{
    const QString l_system_id = m_server->activeAuthSystemId();
    if (l_system_id == QStringLiteral("username")) {
        const QStringList l_login = message.split(" ");
        if (l_login.size() < 2) {
            sendServerMessage("You must specify a username and a password");
            sendServerMessage("Exiting login prompt.");
            logging_in = false;
            return;
        }
        authenticate({l_login[0], l_login[1]});
    }
    else {
        authenticate({message});
    }
    sendServerMessage("Exiting login prompt.");
    logging_in = false;
}

void ClientSession::authenticate(const QStringList &f_args)
{
    if (authenticated) {
        sendServerMessage("You are already logged in!");
        return;
    }

    akashi::AuthThrottle *l_throttle = m_server->authThrottle();
    if (l_throttle->isLockedOut(session_ipid)) {
        sendServerMessage("Too many failed login attempts. Try again in " + QString::number(l_throttle->remainingLockoutSeconds(session_ipid)) + " seconds.");
        return;
    }

    const std::shared_ptr<akashi::Authenticator> l_authenticator = m_server->activeAuthenticator();
    if (!l_authenticator) {
        // Resolution is a boot invariant; a missing system refuses cleanly.
        sendPacket("AUTH", {"0"});
        return;
    }

    const akashi::AuthRequest l_request{clientId(), session_ipid, f_args};
    l_authenticator->authenticate(l_request, std::bind_front(&ServerContext::deliverAuthOutcome, m_server, QPointer<ClientSession>(this)));
}

void ClientSession::onAuthOutcome(const akashi::AuthOutcome &f_outcome)
{
    akashi::AuthThrottle *l_throttle = m_server->authThrottle();
    switch (f_outcome.kind) {
    case akashi::AuthOutcome::Kind::Granted:
        authenticated = true;
        acl_role_id = f_outcome.acl_role_id;
        moderator_name = f_outcome.moderator_name;
        sendPacket("AUTH", {"1"});
        if (m_profile.version.release <= 2 && m_profile.version.major <= 9 && m_profile.version.minor <= 0)
            sendServerMessage("Logged in as a moderator.");
        if (!f_outcome.message.isEmpty())
            sendServerMessage(f_outcome.message);
        l_throttle->recordSuccess(session_ipid);
        break;
    case akashi::AuthOutcome::Kind::Refused:
        sendPacket("AUTH", {"0"});
        if (!f_outcome.message.isEmpty())
            sendServerMessage(f_outcome.message);
        l_throttle->recordFailure(session_ipid);
        break;
    case akashi::AuthOutcome::Kind::Challenge:
        // The hook for multi-round systems; the fields relay verbatim,
        // with no throttle record and no log - nothing was decided yet.
        sendPacket("AUTH", QStringList{QStringLiteral("challenge")} + f_outcome.challenge);
        return;
    }

    // The login log falls back to "Moderator" when the system names
    // nobody, which is how the simple system has always logged.
    m_server->logService()->log({.type = log_type::Login,
                                 .area = m_server->areaById(areaId())->name(),
                                 .char_name = character() + " " + characterName(),
                                 .ooc_name = name(),
                                 .ipid = session_ipid,
                                 .moderator = f_outcome.moderator_name.isEmpty() ? QStringLiteral("Moderator") : f_outcome.moderator_name,
                                 .success = authenticated});
}

QString ClientSession::activeAuthSystemId() const
{
    return m_server ? m_server->activeAuthSystemId() : QString();
}

} // namespace akashi
