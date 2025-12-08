#pragma once

#include <QObject>

class ServiceRegistry;

class Service : public QObject
{
    Q_OBJECT

  public:
    Service(QObject *parent = nullptr) : QObject(parent) {};
    ~Service() = default;

    virtual bool init(ServiceRegistry *registry = nullptr) = 0;
};
