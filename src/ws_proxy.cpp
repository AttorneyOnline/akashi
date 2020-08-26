#include "include/ws_proxy.h"

WSProxy::WSProxy(QObject* parent) : QObject(parent)
{
    server = new QWebSocketServer(QStringLiteral(""),
                                  QWebSocketServer::NonSecureMode, this);
    connect(server, SIGNAL(newConnection()), this, SLOT(wsConnected()));
}

void WSProxy::wsConnected() {}
