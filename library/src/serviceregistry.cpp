#include "serviceregistry.h"
#include "service.h"

#include <QDebug>

Q_LOGGING_CATEGORY(akashiServiceRegistry, "akashi.addon.serviceregistry")

ServiceRegistry::ServiceRegistry(QObject *parent) : QObject{parent}
{
    qCDebug(akashiServiceRegistry) << "Created at" << this;
}

ServiceRegistry::~ServiceRegistry() {}
