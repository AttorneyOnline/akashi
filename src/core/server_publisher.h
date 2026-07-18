#pragma once

#include "akashi_core_export.h"

#include <QList>
#include <QObject>
#include <QSslError>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class ServerSettings;

/**
 * @brief Represents the ServerPublisher of the server. Sends current server information to the serverlist.
 */
class AKASHI_CORE_EXPORT ServerPublisher : public QObject
{
    Q_OBJECT

  public:
    explicit ServerPublisher(int port, int *player_count, ServerSettings *f_settings, QObject *parent = nullptr);
    virtual ~ServerPublisher() = default;

  public Q_SLOTS:

    /**
     * @brief Establishes a connection with masterserver to register or update the listing on the masterserver.
     */
    void publishServer();

    /**
     * @brief Reads the response from the serverlist.
     */
    void finished(QNetworkReply *f_reply);

    /**
     * @brief Logs certificate failures; the request still fails, they are never ignored.
     */
    void onSslErrors(QNetworkReply *f_reply, const QList<QSslError> &f_errors);

  private:
    /**
     * @brief Pointer to the network manager, necessary to execute POST requests to the masterserver.
     */
    QNetworkAccessManager *m_manager;

    /**
     * @brief Advertisers when it expires.
     */
    QTimer *timeout_timer;

    /**
     * @brief The current amount of players on the server.
     */
    int *m_players;

    /**
     * @brief The WS port of the server.
     */
    int m_port;

    ServerSettings *m_settings;
};
