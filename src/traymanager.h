#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>

#include "updatechecker.h"

class UpdateChecker;
class QWidget;

class TrayManager : public QObject {
    Q_OBJECT
public:
    explicit TrayManager(UpdateChecker *checker, QWidget *mainWindow, QObject *parent = nullptr);
    ~TrayManager();

    void show();
    void showNotification(const QString &title, const QString &message,
                          QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                          int durationMs = 5000);

    enum IconState { Idle, Checking, UpdatesAvailable };
    void setIconState(IconState state);

signals:
    void showWindowRequested();
    void checkNowRequested();
    void quitRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onUpdatesFound(const QList<UpdateInfo> &updates);
    void onCheckStarted();
    void onCheckFinished(bool success);

private:
    void buildMenu();
    void createIcon(IconState state);

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QAction *m_openAction;
    QAction *m_checkAction;
    QAction *m_lastCheckAction;
    QAction *m_quitAction;
    QWidget *m_mainWindow;
    UpdateChecker *m_checker;
    IconState m_currentState;
};
