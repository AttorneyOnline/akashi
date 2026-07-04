#include "core/client_session.h"

#include "akashi/event.h"
#include "akashi/log_event.h"
#include "core/arup_broadcaster.h"
#include "core/auth_throttle.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/db_manager.h"
#include "core/event_bus.h"
#include "core/log_service.h"
#include "core/permission_registry.h"
#include "core/player_state_observer.h"
#include "core/server_context.h"
#include "core/server_settings.h"
#include "core/text_filter_registry.h"
#include "proto/evidence.h"
#include "proto/ic.h"
#include "proto/packet.h"
#include "proto/text_utils.h"
#include "world/area.h"
#include "world/jukebox.h"
#include "world/rule_registry.h"

#include <QFutureWatcher>
#include <QPointer>
#include <QQueue>
#include <QtConcurrent/QtConcurrentRun>

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
    if (transport->isOpen()) {
        transport->write(f_packet);
        return;
    }

    if (pending_packets.size() >= PENDING_PACKET_LIMIT) {
        pending_packets.dequeue();
        pending_overflowed = true;
    }
    pending_packets.enqueue(f_packet);
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
    qDebug() << remote_ip.toString() << "disconnected";
#endif
    // A client that never joined has no presence to withdraw - and a rejected
    // (banned) connection must not broadcast ARUPs to the whole server.
    if (has_joined) {
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
    qDebug() << "Received packet:" << packet.header() << ":" << packet.fields() << "args length:" << packet.fieldCount();
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
    if (f_header != "CH" && has_joined) {
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
    l_ctx.player_id = clientId();
    l_ctx.area_id = new_area;
    l_ctx.floor_id = l_new_floor;
    l_ctx.services = m_server->services();
    l_ctx.payload = {
        {QStringLiteral("lock_status"),
         l_target->lockState() == akashi::Area::LockState::Locked ? QStringLiteral("locked") : l_target->lockState() == akashi::Area::LockState::Spectatable ? QStringLiteral("spectatable")
                                                                                                                                                             : QStringLiteral("free")},
        {QStringLiteral("is_invited"), l_target->invited().contains(clientId())},
        {QStringLiteral("bypass_locks"), canPerform(permission::bypass_locks)},
        {QStringLiteral("area_name"), m_server->areaName(new_area)},
        {QStringLiteral("character_id"), m_server->characterId(character())},
    };

    const akashi::Floor *l_target_floor = m_server->floorById(l_new_floor);
    akashi::RuleVerdict l_verdict = akashi::RuleRegistry::checkBefore(
        akashi::AreaEvents::PlayerJoined, l_ctx,
        l_target->beforeRules(),
        l_target_floor ? l_target_floor->before_rules : QVector<akashi::BeforeRuleEntry>{});
    if (!l_verdict.allowed) {
        sendServerMessage(l_verdict.reason);
        return;
    }

    if (character() != "") {
        m_server->areaById(areaId())->changeCharacter(m_server->characterId(character()), -1);
        m_server->updateCharsTaken(m_server->areaById(areaId()));
    }
    m_server->areaById(areaId())->removeClient(player()->char_id, clientId());
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

std::optional<QString> ClientSession::checkBeforeRule(const QString &f_event, const QVariantMap &f_payload)
{
    akashi::RuleContext l_ctx;
    l_ctx.player_id = clientId();
    l_ctx.area_id = areaId();
    l_ctx.floor_id = m_server->floorIdForArea(areaId());
    l_ctx.services = m_server->services();
    l_ctx.payload = f_payload;
    akashi::Area *l_area = m_server->areaById(areaId());
    const akashi::Floor *l_floor = m_server->floorById(l_ctx.floor_id);
    akashi::RuleVerdict l_verdict = akashi::RuleRegistry::checkBefore(f_event, l_ctx,
                                                                      l_area ? l_area->beforeRules() : QVector<akashi::BeforeRuleEntry>{},
                                                                      l_floor ? l_floor->before_rules : QVector<akashi::BeforeRuleEntry>{});
    if (!l_verdict.allowed)
        return l_verdict.reason;
    return std::nullopt;
}

void ClientSession::runAfterRule(const QString &f_event, const QVariantMap &f_payload)
{
    akashi::RuleContext l_ctx;
    l_ctx.player_id = clientId();
    l_ctx.area_id = areaId();
    l_ctx.floor_id = m_server->floorIdForArea(areaId());
    l_ctx.services = m_server->services();
    l_ctx.payload = f_payload;
    akashi::Area *l_area = m_server->areaById(areaId());
    const akashi::Floor *l_floor = m_server->floorById(l_ctx.floor_id);
    akashi::RuleRegistry::runAfter(f_event, l_ctx,
                                   l_area ? l_area->afterRules() : QVector<akashi::AfterRuleEntry>{},
                                   l_floor ? l_floor->after_rules : QVector<akashi::AfterRuleEntry>{});
}

bool ClientSession::changeCharacter(int char_id)
{
    akashi::Area *l_area = m_server->areaById(areaId());

    if (char_id >= m_server->characterCount()) {
        return false;
    }

    if (isCharCursed() && !charcurse_list.contains(char_id)) {
        return false;
    }

    bool l_successfulChange = l_area->changeCharacter(m_server->characterId(character()), char_id);

    if (char_id < 0) {
        setCharacter("");
        player()->char_id = char_id;
        setSpectator(true);
    }

    if (l_successfulChange == true) {
        QString l_char_selected = m_server->characterById(char_id);
        setCharacter(l_char_selected);
        player()->pos = "";
        m_server->updateCharsTaken(l_area);
        sendPacket("PV", {QString::number(clientId()), "CID", QString::number(char_id)});
        runAfterRule(akashi::AreaEvents::CharacterChanged, {{QStringLiteral("character"), l_char_selected}});
        return true;
    }
    return false;
}

void ClientSession::changePosition(QString new_pos)
{
    player()->pos = new_pos;
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
            const QStringList l_permissions = l_variant ? l_variant->permissions : l_spec->permissions;
            if (!l_permissions.isEmpty()) {
                bool l_has_permission = false;
                for (const QString &l_perm : l_permissions) {
                    if (canPerform(l_perm)) {
                        l_has_permission = true;
                        break;
                    }
                }
                if (!l_has_permission) {
                    sendServerMessage("You do not have permission to use that command.");
                    return;
                }
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

bool ClientSession::isJoined() const
{
    return has_joined;
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
        if (!identified) {
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
    return identified;
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
    identified = true;
    if (packets) {
        codecs = packets->codecs().resolve(m_profile);
    }
}

void ClientSession::markJoined()
{
    has_joined = true;
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
QStringList ClientSession::wordFilters() const { return m_server->filterList(); }
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
    if (changeCharacter(f_char_id)) {
        player()->char_id = f_char_id;
    }
    if (player()->char_id > SPECTATOR_ID) {
        setSpectator(false);
    }
    return player()->char_id == f_char_id;
}

bool ClientSession::canUseOocChat() const
{
    return !hasSanction(akashi::Sanction::OocMuted);
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

bool ClientSession::isEvidenceHiddenCm() const
{
    return m_server->areaById(areaId())->evidenceAccess() == akashi::EvidenceStore::Access::HiddenCm;
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
    l_evidence.description = f_description;
    l_evidence.image = f_image;
    l_area->replaceEvidence(l_real_index, l_evidence);
}

void ClientSession::setCasingPreferences(const QList<bool> &f_preferences)
{
    casing_preferences = f_preferences;
}

bool ClientSession::canUseIcChat() const
{
    return !hasSanction(akashi::Sanction::Muted);
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
    if (player()->pos != f_position) {
        player()->pos = f_position;
        player()->pos.replace("../", "").replace("..\\", "");
        updateEvidenceList(m_server->areaById(areaId()));
    }
}

std::optional<QString> ClientSession::applyTextFilters(const QString &f_text) const
{
    QSet<QString> l_ids = sanctions;
    if (m_server->areaById(areaId())->isMedievalMode())
        l_ids.insert(akashi::Sanction::Medieval);
    return m_server->textFilterRegistry()->apply(f_text, l_ids);
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
    return hasSanction(akashi::Sanction::CharCurse);
}

void ClientSession::setCharCursed(bool f_char_cursed)
{
    setSanction(akashi::Sanction::CharCurse, f_char_cursed);
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

void ClientSession::setCharacterId(int f_char_id)
{
    player()->char_id = f_char_id;
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

bool ClientSession::isShoutAllowed() const
{
    return m_server->areaById(areaId())->isShoutAllowed();
}

bool ClientSession::isShownameAllowed() const
{
    return m_server->areaById(areaId())->isShownameAllowed();
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
    player()->pairing_with = f_pair_id;
    akashi::PairInfo l_pair;
    akashi::Area *l_area = m_server->areaById(areaId());
    const QList<int> l_joined = l_area->players();
    for (int l_client_id : l_joined) {
        ClientSession *l_client = m_server->clientById(l_client_id);
        if (l_client == nullptr) {
            continue;
        }
        if (l_client->pairingWith() == player()->char_id && f_pair_id != player()->char_id && l_client->characterId() == player()->pairing_with && l_client->pos() == player()->pos) {
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

        const QRegularExpression jump("(?<arrow>>|<)(?<int>\\d+)");
        const QRegularExpressionMatch match = jump.match(l_args[4]);

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
            player()->pos = l_jump->first.side();
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

    const akashi::Packet l_classic("MS", l_fields);
    const QVector<ClientSession *> l_clients = m_server->clients();
    for (ClientSession *l_client : l_clients) {
        if (l_client->areaId() != areaId()) {
            continue;
        }
        QStringList l_client_fields = l_fields;
        if (l_evidence_presented) {
            l_client_fields[11] = QString::number(l_area->visibleIndexByEvidenceIndex(l_real_index, l_client->pos(), l_client->canPerform(permission::gamemaster)));
        }
        if (l_evidence_presented) {
            l_client->sendPacket(akashi::Packet("MS", l_client_fields));
        }
        else {
            l_client->sendPacket(l_classic);
        }
    }

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
    return hasSanction(akashi::Sanction::DjBlocked);
}

bool ClientSession::isJukeboxEnabled() const
{
    return m_server->areaById(areaId())->isJukeboxEnabled();
}

QString ClientSession::queueJukeboxSong(const QString &f_song)
{
    return m_server->areaById(areaId())->jukebox()->queueSong(clientId(), f_song);
}

QString ClientSession::resolveSongAlias(const QString &f_song)
{
    const auto l_info = m_server->areaById(areaId())->jukebox()->songInfo(f_song);
    return l_info ? l_info->real_name : f_song;
}

void ClientSession::recordMusicChange(const QString &f_song)
{
    akashi::Area *l_area = m_server->areaById(areaId());
    m_server->logService()->log({.type = log_type::Music,
                                 .area = l_area->name(),
                                 .char_name = character() + " " + characterName(),
                                 .ooc_name = name(),
                                 .ipid = session_ipid,
                                 .message = f_song});

    // An empty showname would show as "played by ." in /currentmusic.
    if (characterName().isEmpty()) {
        l_area->changeMusic(character(), f_song);
        return;
    }
    l_area->changeMusic(characterName(), f_song);
}

bool ClientSession::isWtceBlocked() const
{
    return hasSanction(akashi::Sanction::WtceBlocked);
}

bool ClientSession::isWtceAllowed() const
{
    return m_server->areaById(areaId())->isWtceAllowed();
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

void ClientSession::logJudgeAction(const QString &f_action)
{
    updateJudgeLog(m_server->areaById(areaId()), this, f_action);
}

void ClientSession::addEvidence(const QString &f_name, const QString &f_description, const QString &f_image)
{
    akashi::Evidence l_evidence;
    l_evidence.name = f_name;
    l_evidence.description = f_description;
    l_evidence.image = f_image;
    akashi::Area *l_area = m_server->areaById(areaId());
    l_area->appendEvidence(l_evidence);
    sendEvidenceList(l_area);
}

void ClientSession::broadcastArea(const akashi::Packet &f_packet)
{
    m_server->broadcast(f_packet, areaId());
}

void ClientSession::setPenalty(int f_bar, int f_value)
{
    akashi::Area *l_area = m_server->areaById(areaId());
    if (f_bar == 1) {
        l_area->changeHP(akashi::Area::Side::DEFENCE, f_value);
    }
    else if (f_bar == 2) {
        l_area->changeHP(akashi::Area::Side::PROSECUTOR, f_value);
    }
}

int ClientSession::penalty(int f_bar) const
{
    akashi::Area *l_area = m_server->areaById(areaId());
    return f_bar == 1 ? l_area->defHP() : l_area->proHP();
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
    l_query.auth_type = m_server->authType() == AuthType::SIMPLE
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

    akashi::ModcallEvent l_event{
        .client_id = clientId(),
        .area_id = areaId(),
        .area_name = l_area_name,
        .char_name = character(),
        .ooc_name = name(),
        .ipid = session_ipid,
        .reason = f_reason};
    m_server->eventBus()->notify(l_event);
}

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
    for (ClientSession *l_client : l_clients) {
        l_client->sendPacket("KK", {f_reason});
        l_client->closeSocket();
    }

    m_server->logService()->log({.type = log_type::Kick,
                                 .message = f_reason,
                                 .moderator = l_moderator_name,
                                 .target_ipid = l_target->ipid()});
    sendServerMessage("Kicked " + QString::number(l_clients.size()) + " client(s) with ipid " + l_target->ipid() + " for reason: " + f_reason);
}

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

    DBManager::BanInfo l_ban;
    l_ban.ip = l_target->remoteIp();
    l_ban.ipid = l_target->ipid();
    l_ban.moderator = l_moderator_name;
    l_ban.reason = f_reason;
    l_ban.time = QDateTime::currentDateTime().toSecsSinceEpoch();

    QString l_timestamp;
    if (f_duration == -1) {
        l_ban.duration = -2;
        l_timestamp = "permanently";
    }
    else {
        l_ban.duration = f_duration * 60;
        l_timestamp = QDateTime::fromSecsSinceEpoch(l_ban.time).addSecs(l_ban.duration).toString("MM/dd/yyyy, hh:mm");
    }

    const QList<ClientSession *> l_clients = m_server->clientsByIpid(l_target->ipid());
    for (ClientSession *l_client : l_clients) {
        l_ban.hdid = l_client->hwid();
        m_server->databaseManager()->addBan(l_ban);
        l_client->sendPacket("KB", {f_reason});
        l_client->closeSocket();
    }

    m_server->logService()->log({.type = log_type::Ban,
                                 .message = f_reason,
                                 .moderator = l_moderator_name,
                                 .target_ipid = l_target->ipid(),
                                 .duration = l_timestamp});
    sendServerMessage("Banned " + QString::number(l_clients.size()) + " client(s) with ipid " + l_target->ipid() + " for reason: " + f_reason);

    const int l_ban_id = m_server->databaseManager()->banId(l_ban.ip);
    akashi::BanIssuedEvent l_event{
        .ban_id = l_ban_id,
        .moderator = l_ban.moderator,
        .target_ipid = l_ban.ipid,
        .duration = l_timestamp,
        .reason = l_ban.reason};
    m_server->eventBus()->notify(l_event);
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

void ClientSession::loginAttempt(QString message)
{
    akashi::AuthThrottle *l_throttle = m_server->authThrottle();
    if (l_throttle->isLockedOut(session_ipid)) {
        sendServerMessage("Too many failed login attempts. Try again in " + QString::number(l_throttle->remainingLockoutSeconds(session_ipid)) + " seconds.");
        sendServerMessage("Exiting login prompt.");
        logging_in = false;
        return;
    }

    switch (m_server->authType()) {
    case AuthType::SIMPLE:
        if (message == m_server->serverSettings()->modpass()) {
            sendPacket("AUTH", {"1"});
            if (m_profile.version.release <= 2 && m_profile.version.major <= 9 && m_profile.version.minor <= 0)
                sendServerMessage("Logged in as a moderator.");
            authenticated = true;
            acl_role_id = ACLRolesHandler::SUPER_ID;
            l_throttle->recordSuccess(session_ipid);
        }
        else {
            sendPacket("AUTH", {"0"});
            sendServerMessage("Incorrect password.");
            l_throttle->recordFailure(session_ipid);
        }
        m_server->logService()->log({.type = log_type::Login,
                                     .area = m_server->areaById(areaId())->name(),
                                     .char_name = character() + " " + characterName(),
                                     .ooc_name = name(),
                                     .ipid = session_ipid,
                                     .moderator = QStringLiteral("Moderator"),
                                     .success = authenticated});
        break;
    case AuthType::ADVANCED:
    {
        QStringList l_login = message.split(" ");
        if (l_login.size() < 2) {
            sendServerMessage("You must specify a username and a password");
            sendServerMessage("Exiting login prompt.");
            logging_in = false;
            return;
        }
        QString l_username = l_login[0];
        QString l_password = l_login[1];

        auto l_creds = m_server->databaseManager()->fetchCredentials(l_username);
        if (!l_creds) {
            sendPacket("AUTH", {"0"});
            sendServerMessage("Incorrect password.");
            l_throttle->recordFailure(session_ipid);
            m_server->logService()->log({.type = log_type::Login,
                                         .area = m_server->areaById(areaId())->name(),
                                         .char_name = character() + " " + characterName(),
                                         .ooc_name = name(),
                                         .ipid = session_ipid,
                                         .moderator = l_username,
                                         .success = false});
            break;
        }

        sendServerMessage("Exiting login prompt.");
        logging_in = false;

        QString l_salt = l_creds->salt;
        QString l_stored_hash = l_creds->stored_hash;
        QString l_acl_role = l_creds->acl_role;
        bool l_needs_rehash = QByteArray::fromHex(l_salt.toUtf8()).length() < CryptoHelper::pbkdf2_salt_len;

        QFuture<QString> l_future = QtConcurrent::run([l_salt, l_password]() {
            return CryptoHelper::hash_password(QByteArray::fromHex(l_salt.toUtf8()), l_password);
        });

        auto *l_watcher = new QFutureWatcher<QString>(this);
        QPointer<ClientSession> l_guard(this);
        connect(l_watcher, &QFutureWatcher<QString>::finished, this,
                [l_guard, l_watcher, l_username, l_password, l_stored_hash, l_acl_role, l_needs_rehash]() {
                    l_watcher->deleteLater();
                    if (!l_guard)
                        return;
                    ClientSession *l_self = l_guard.data();

                    const QString l_computed = l_watcher->result();
                    const bool l_matches = CryptoHelper::constantTimeEquals(l_computed, l_stored_hash);

                    akashi::AuthThrottle *l_throttle = l_self->m_server->authThrottle();
                    if (l_matches) {
                        l_self->authenticated = true;
                        l_self->acl_role_id = l_acl_role;
                        l_self->moderator_name = l_username;
                        l_self->sendPacket("AUTH", {"1"});
                        if (l_self->m_profile.version.release <= 2 && l_self->m_profile.version.major <= 9 && l_self->m_profile.version.minor <= 0)
                            l_self->sendServerMessage("Logged in as a moderator.");
                        l_self->sendServerMessage("Welcome, " + l_username);
                        l_throttle->recordSuccess(l_self->session_ipid);

                        if (l_needs_rehash)
                            l_self->m_server->databaseManager()->updatePassword(l_username, l_password);
                    }
                    else {
                        l_self->sendPacket("AUTH", {"0"});
                        l_self->sendServerMessage("Incorrect password.");
                        l_throttle->recordFailure(l_self->session_ipid);
                    }

                    l_self->m_server->logService()->log({.type = log_type::Login,
                                                         .area = l_self->m_server->areaById(l_self->areaId())->name(),
                                                         .char_name = l_self->character() + " " + l_self->characterName(),
                                                         .ooc_name = l_self->name(),
                                                         .ipid = l_self->session_ipid,
                                                         .moderator = l_username,
                                                         .success = l_self->authenticated});
                });
        l_watcher->setFuture(l_future);
        return;
    }
    }
    sendServerMessage("Exiting login prompt.");
    logging_in = false;
    return;
}

} // namespace akashi
