#pragma once

#include <QMap>
#include <QObject>
#include <memory>

class Packet;

class ServerClient : public QObject
{
    Q_OBJECT
public:
    explicit ServerClient(QObject *parent = nullptr);

private:
    QMap<QString, std::function<void(std::shared_ptr<Packet>)>> m_handlers;
    QString m_version;

signals:
};
