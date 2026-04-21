// AI-generated: written by Claude.
#include "aoclient.h"
#include "network/network_socket.h"
#include "playerstateobserver.h"

#include <QTest>
#include <QWebSocket>

namespace tests {
namespace unittests {

class tst_PlayerStateObserver : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void unregisterUnknownClient();
    void registerAndUnregister();

  private:
    AOClient *makeClient(int id);
};

// The clients leak on purpose, AOClient cannot be destroyed without a server (fixed in M6).
AOClient *tst_PlayerStateObserver::makeClient(int id)
{
    NetworkSocket *socket = new NetworkSocket(new QWebSocket());
    return new AOClient(nullptr, socket, nullptr, id, nullptr);
}

void tst_PlayerStateObserver::unregisterUnknownClient()
{
    PlayerStateObserver observer;
    AOClient *client = makeClient(0);

    // A client that never registered must be ignored.
    observer.unregisterClient(client);
    observer.unregisterClient(client);
}

void tst_PlayerStateObserver::registerAndUnregister()
{
    PlayerStateObserver observer;
    AOClient *client = makeClient(0);

    observer.registerClient(client);
    observer.unregisterClient(client);

    // A second unregister must be ignored.
    observer.unregisterClient(client);
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_PlayerStateObserver)

#include "tst_playerstateobserver.moc"
