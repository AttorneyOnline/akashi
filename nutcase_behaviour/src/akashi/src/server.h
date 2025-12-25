#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#include "serviceregistry.h"
#include "servicewrapper.h"

class QNetworkAccessManager;
class DiscordHook;
class PluginManager;
class PacketRegistry;

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);

    void initializeServices();

Q_SIGNALS:
    void shutdownRequested();

private:
    ServiceRegistry *m_registry;
    DiscordHook *m_discord_hook;
    PluginManager *m_plugins;
    PacketRegistry *m_packet_registry;
    ServiceWrapper<QNetworkAccessManager> *m_network_service;
};
