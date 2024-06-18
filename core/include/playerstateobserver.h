#pragma once

class AOClient;

#include <QMap>
#include <QObject>

class PlayerStateObserver : public QObject
{
public:
    PlayerStateObserver(QObject* parent);

    void registerClient(AOClient* client);
    void deregisterClient(int id);

private Q_SLOTS:
    void playerChangedArea();
    void displayNameChanged();
    void characterChanged();

private:
    QMap<int, AOClient*> observed_clients;
};
