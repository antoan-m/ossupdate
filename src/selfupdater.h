#pragma once

#include <QObject>
#include <QVersionNumber>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QProcess>

class PasswordManager;

class SelfUpdater : public QObject {
    Q_OBJECT
public:
    explicit SelfUpdater(PasswordManager *passwordManager, QObject *parent = nullptr);

    void start();
    void checkNow();
    void downloadAndInstall();
    void providePassword(const QString &pwd);
    void skipVersion();

    QVersionNumber currentVersion() const;
    QVersionNumber latestVersion() const;
    QString latestDownloadUrl() const;
    bool isChecking() const;
    bool isUpdateAvailable() const;

signals:
    void checkFinished(bool success);
    void updateAvailable(const QString &version, const QString &downloadUrl);
    void updateNotAvailable();
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &localPath);
    void installFinished(bool success, const QString &message);
    void passwordRequired();

private slots:
    void onApiReplyFinished();
    void onDownloadFinished();
    void onInstallFinished();

private:
    void parseApiResponse(const QByteArray &data);
    void startDownload(const QString &url);
    void startInstall(const QString &pwd);

    PasswordManager *m_passwordManager;
    QNetworkAccessManager *m_network;
    QTimer *m_timer;
    QProcess *m_process;

    QVersionNumber m_currentVersion;
    QVersionNumber m_latestVersion;
    QString m_latestDownloadUrl;
    QString m_downloadedPath;
    bool m_checking;
};
