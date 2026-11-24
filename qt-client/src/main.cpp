#include "Library.h"
#include "Mpris.h"

#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkProxy>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QStandardPaths>

namespace {
// Covers are fetched by QML Image items. A shared disk cache keeps them across pages and
// restarts, so revisiting a page shows its artwork immediately instead of reloading it.
class CachedNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override
    {
        auto *manager = new QNetworkAccessManager(parent);
        auto *cache = new QNetworkDiskCache(manager);
        cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/images");
        cache->setMaximumCacheSize(512LL * 1024 * 1024);
        manager->setCache(cache);
        manager->setProxy(QNetworkProxy::NoProxy);
        return manager;
    }
};
}

namespace {
// One client per user session. A second launch asks the running one to show itself and
// exits, so double-clicking the launcher never opens a duplicate window.
QString instanceName()
{
    return QStringLiteral("record-room-") + qEnvironmentVariable("USER", QStringLiteral("user"))
        + qEnvironmentVariable("RECORD_ROOM_INSTANCE");
}

bool activateRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(instanceName());
    if (!socket.waitForConnected(400))
        return false;
    socket.write("show\n");
    socket.waitForBytesWritten(400);
    return true;
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("RecordRoom");
    app.setApplicationName("RecordRoom");
    if (activateRunningInstance())
        return 0;
    QLocalServer instanceServer;
    if (!instanceServer.listen(instanceName())) {
        // A crashed instance can leave a stale socket file behind.
        QLocalServer::removeServer(instanceName());
        instanceServer.listen(instanceName());
    }
    app.setApplicationDisplayName(QStringLiteral("网易云音乐"));
    app.setDesktopFileName("local.record-room");
    QIcon icon;
    for (const int size : {16, 32, 48, 64, 128})
        icon.addFile(QStringLiteral(":/branding/assets/icons/app-%1.png").arg(size), QSize(size, size));
    icon.addFile(QStringLiteral(":/branding/assets/netease-music.png"), QSize(256, 256));
    app.setWindowIcon(icon);
    QQuickStyle::setStyle("Basic");
    // The desktop lyric strip is a see-through window; it needs an alpha channel.
    QQuickWindow::setDefaultAlphaBuffer(true);

    Library library;
    QQmlApplicationEngine engine;
    CachedNetworkFactory networkFactory;
    engine.setNetworkAccessManagerFactory(&networkFactory);
    engine.setInitialProperties({{"library", QVariant::fromValue(&library)}});
    // Lets small leaf components warm caches (e.g. on hover) without threading the object through.
    engine.rootContext()->setContextProperty(QStringLiteral("prefetcher"), &library);
    engine.loadFromModule("RecordRoom", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    const auto showWindow = [&engine] {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
        if (window) {
            if (window->visibility() == QWindow::Minimized || !window->isVisible())
                window->showNormal();
            window->raise();
            window->requestActivate();
            window->alert(0);
        }
    };
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &app, [&instanceServer, showWindow] {
        while (auto *client = instanceServer.nextPendingConnection()) {
            QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
            showWindow();
        }
    });

    // System media controls, media keys and desktop lyric tools (desktop-lyric, osdlyrics).
    Mpris mpris(&library, showWindow, [&engine] {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0)))
            window->close();
        else
            QCoreApplication::quit();
    });

    library.start();
    return app.exec();
}
