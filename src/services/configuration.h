#pragma once

#include <QObject>
#include <service.h>

class FileSystemSupervisor;

class Configuration : public Service
{
    Q_OBJECT

  public:
    explicit Configuration(QObject *parent = nullptr);
};
