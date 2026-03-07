#include "service.h"

#include <QDebug>

Q_LOGGING_CATEGORY(akashiService, "akashi.addon.service")

void Service::setServiceRegistry(ServiceRegistry *f_registry)
{
    qCDebug(akashiService) << "Called dummy implementation of" << Q_FUNC_INFO;
    qCDebug(akashiService) << "This is likely not what is being intended.";
}

void Service::setState(State f_state)
{
    m_state = f_state;
    qCDebug(akashiService) << "Status of" << getServiceProperty("identifier") << "is set to" << m_state;
}

Service::State Service::getState()
{
    qCDebug(akashiService) << "Status of" << getServiceProperty("identifier") << "is" << m_state;
    return m_state;
}

QString Service::getServiceProperty(QString f_key) const
{
    return m_service_properties.value(f_key, {});
}
