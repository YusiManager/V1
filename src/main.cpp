#include <QtWidgets>
#include <QGuiApplication>
#include <QSessionManager>
#include <QLocalServer>
#include <QLocalSocket>
#include "mainwindow.h"
#include "single_instance.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Yusi");
    QApplication::setApplicationName("YusiManager");
    QApplication::setApplicationDisplayName("YusiManager.com");
    QApplication::setDesktopFileName("yusimanager");
    QApplication::setWindowIcon(QIcon(":/yusimanager.svg"));
    QApplication::setQuitOnLastWindowClosed(false);

#if QT_CONFIG(sessionmanager)
    QObject::connect(&app, &QGuiApplication::commitDataRequest,
                     [&](QSessionManager &sm){ sm.setRestartHint(QSessionManager::RestartNever); });
    QObject::connect(&app, &QGuiApplication::saveStateRequest,
                     [&](QSessionManager &sm){ sm.setRestartHint(QSessionManager::RestartNever); });
#endif

    QString serverName = instanceServerName();
    if (notifyRunningInstance(serverName, "ACTIVATE")) return 0;

    QLocalServer server;
    QLocalServer::removeServer(serverName);
    if (!server.listen(serverName)) {
        QLocalServer::removeServer(serverName);
        server.listen(serverName);
    }

    bool startHidden = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]).trimmed();
        if (a == "--minimized" || a == "--hidden") { startHidden = true; break; }
    }

    QString dir;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            const QString a = QString::fromLocal8Bit(argv[i]).trimmed();
            if (a.startsWith("--")) continue;
            dir = a;
            break;
        }
    } else {
        QString localSites = QDir::current().filePath("servers");
        QString homeSites  = QDir::home().filePath("servers");
        if (QDir(localSites).exists()) dir = localSites;
        else if (QDir(homeSites).exists()) dir = homeSites;
        else dir = QDir::currentPath();
    }

    MainWindow w(dir);
    w.setWindowTitle("v1.0");
    w.setWindowIcon(qApp->windowIcon());

    if (!startHidden) {
        w.show();
    } else {
        w.setWindowState(Qt::WindowMinimized);
        QTimer::singleShot(0, &w, [&w]{ w.hide(); });
    }

    QObject::connect(&server, &QLocalServer::newConnection, &w, [&]{
        while (QLocalSocket *client = server.nextPendingConnection()) {
            client->waitForReadyRead(200);
            QByteArray msg = client->readAll();
            client->disconnectFromServer();
            if (msg.contains("ACTIVATE")) w.bringToFront();
        }
    });

    return app.exec();
}
