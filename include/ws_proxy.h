#ifndef WS_PROXY_H
#define WS_PROXY_H

#include <QMap>
#include <QTcpSocket>
#include <QtWebSockets/QtWebSockets>

class WSProxy : public QObject {
    Q_OBJECT
  public:
    WSProxy(QObject* parent);

  public slots:
    void wsConnected();

  private:
    QWebSocketServer* server;
    QMap<QWebSocket*, QTcpSocket*> tcp_sockets;
    QMap<QTcpSocket*, QWebSocket*> web_sockets;
};

#endif // WS_PROXY_H
