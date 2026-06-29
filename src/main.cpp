#include <QApplication>
#include <QFile>
#include <QStyleHints>
#include <QIcon>

#include "settings.h"
#include "passwordmanager.h"
#include "autostartmanager.h"
#include "lockmanager.h"
#include "updatechecker.h"
#include "traymanager.h"
#include "mainwindow.h"

static QString loadStyleSheet(bool dark)
{
    QString path = dark ? QStringLiteral(":/styles/dark.qss")
                        : QStringLiteral(":/styles/light.qss");
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(file.readAll());
    return QString();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("opensuse-update-applet"));
    app.setApplicationDisplayName(QStringLiteral("openSUSE Update Applet"));
    app.setOrganizationName(QStringLiteral("OpenSUSE"));
    app.setQuitOnLastWindowClosed(false);

    bool darkMode = app.styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    QString styleSheet = loadStyleSheet(darkMode);
    if (!styleSheet.isEmpty())
        app.setStyleSheet(styleSheet);

    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.svg")));

    Settings::instance();

    auto *passwordManager = new PasswordManager();
    auto *autostartManager = new AutostartManager();
    auto *lockManager = new LockManager(passwordManager);
    auto *updateChecker = new UpdateChecker(passwordManager);

    auto *mainWindow = new MainWindow(
        updateChecker, lockManager, passwordManager, autostartManager);

    auto *trayManager = new TrayManager(updateChecker, mainWindow);

    QObject::connect(trayManager, &TrayManager::showWindowRequested, [mainWindow]() {
        mainWindow->show();
        mainWindow->raise();
        mainWindow->activateWindow();
    });

    QObject::connect(trayManager, &TrayManager::checkNowRequested, [updateChecker]() {
        updateChecker->checkNow();
    });

    QObject::connect(trayManager, &TrayManager::quitRequested, [&app]() {
        app.quit();
    });

    QObject::connect(mainWindow, &MainWindow::closeRequested, [mainWindow]() {
        mainWindow->hide();
    });

    mainWindow->hide();
    trayManager->show();

    updateChecker->start();

    return app.exec();
}
