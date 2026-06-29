#pragma once

#include <QObject>
#include <QTimer>
#include <QProcess>
#include <QDateTime>
#include <QStringList>

class PasswordManager;

struct UpdateInfo {
    QString name;
    QString repository;
    QString oldVersion;
    QString newVersion;
    QString type; // "zypper", "flatpak", "snap"
};

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(PasswordManager *passwordManager, QObject *parent = nullptr);

    void start();
    void checkNow();
    void installAll();
    void installSelected(const QStringList &packageNames);
    void installFlatpakUpdates();
    void installSnapUpdates();
    void providePassword(const QString &password);

    QList<UpdateInfo> updates() const;
    int zypperCount() const;
    int flatpakCount() const;
    int snapCount() const;
    int totalCount() const;
    bool isChecking() const;
    bool isInstalling() const;
    QDateTime lastCheckTime() const;

    void setCheckInterval(int minutes);

signals:
    void checkStarted();
    void checkFinished(bool success);
    void updatesFound(const QList<UpdateInfo> &updates);
    void installProgress(int percent);
    void installOutput(const QString &line);
    void installFinished(bool success, const QString &message);
    void errorOccurred(const QString &message);
    void passwordRequired();

private slots:
    void onZypperListFinished();
    void onFlatpakListFinished();
    void onSnapListFinished();
    void onInstallProcessFinished();

private:
    void runZypperListUpdates();
    void runFlatpakListUpdates();
    void runSnapListUpdates();
    void allChecksDone();
    void doInstall(const QString &pwd, const QStringList &pkgs = {});
    void startTimer();

    QList<UpdateInfo> parseZypperUpdates(const QByteArray &output);
    QList<UpdateInfo> parseFlatpakUpdates(const QByteArray &output);
    QList<UpdateInfo> parseSnapUpdates(const QByteArray &output);

    PasswordManager *m_passwordManager;
    QTimer *m_timer;
    QProcess *m_process;

    QList<UpdateInfo> m_allUpdates;
    QList<UpdateInfo> m_zypperUpdates;
    QList<UpdateInfo> m_flatpakUpdates;
    QList<UpdateInfo> m_snapUpdates;

    int m_pendingChecks;
    bool m_checking;
    bool m_installing;
    bool m_waitingForPassword;
    QDateTime m_lastCheckTime;

    QStringList m_pendingInstallPackages;
};
