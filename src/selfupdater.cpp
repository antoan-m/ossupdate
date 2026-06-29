#include "selfupdater.h"
#include "passwordmanager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

SelfUpdater::SelfUpdater(PasswordManager *passwordManager, QObject *parent)
    : QObject(parent)
    , m_passwordManager(passwordManager)
    , m_network(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
    , m_process(nullptr)
    , m_currentVersion(QVersionNumber::fromString(QStringLiteral(APP_VERSION)))
    , m_checking(false)
{
    m_timer->setInterval(86400000);
    connect(m_timer, &QTimer::timeout, this, &SelfUpdater::checkNow);
}

void SelfUpdater::start()
{
    m_timer->start();
    checkNow();
}

void SelfUpdater::checkNow()
{
    if (m_checking)
        return;
    m_checking = true;

    QUrl url(QStringLiteral("https://api.github.com/repos/antoan-m/opensuse_autoupdate_applet/releases/latest"));
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "opensuse-update-applet/" APP_VERSION);

    QNetworkReply *reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, &SelfUpdater::onApiReplyFinished);
}

void SelfUpdater::onApiReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    m_checking = false;

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFinished(false);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    parseApiResponse(data);
}

void SelfUpdater::parseApiResponse(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit checkFinished(false);
        return;
    }

    QJsonObject root = doc.object();
    QString tagName = root.value(QStringLiteral("tag_name")).toString();

    if (tagName.startsWith(QStringLiteral("v")))
        tagName = tagName.mid(1);

    m_latestVersion = QVersionNumber::fromString(tagName);
    if (m_latestVersion.isNull()) {
        emit checkFinished(false);
        return;
    }

    QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    m_latestDownloadUrl.clear();
    for (const QJsonValue &val : assets) {
        QJsonObject asset = val.toObject();
        QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".rpm"))) {
            m_latestDownloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
            break;
        }
    }

    if (m_latestDownloadUrl.isEmpty()) {
        emit checkFinished(false);
        return;
    }

    emit checkFinished(true);

    if (m_latestVersion > m_currentVersion) {
        emit updateAvailable(tagName, m_latestDownloadUrl);
    } else {
        emit updateNotAvailable();
    }
}

void SelfUpdater::startDownload(const QString &url)
{
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "opensuse-update-applet/" APP_VERSION);

    QNetworkReply *reply = m_network->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, &SelfUpdater::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, &SelfUpdater::onDownloadFinished);
}

void SelfUpdater::onDownloadFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    if (reply->error() != QNetworkReply::NoError) {
        emit installFinished(false, QStringLiteral("Download failed: ") + reply->errorString());
        reply->deleteLater();
        return;
    }

    QFile file(m_downloadedPath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit installFinished(false, QStringLiteral("Cannot write to ") + m_downloadedPath);
        reply->deleteLater();
        return;
    }
    file.write(reply->readAll());
    file.close();
    reply->deleteLater();

    emit downloadFinished(m_downloadedPath);

    m_passwordManager->getPassword([this](const QString &pwd) {
        if (pwd.isEmpty()) {
            emit passwordRequired();
            return;
        }
        startInstall(pwd);
    });
}

void SelfUpdater::onInstallFinished()
{
    if (!m_process)
        return;

    int exitCode = m_process->exitCode();
    QString err = QString::fromUtf8(m_process->readAllStandardError());
    m_process->deleteLater();
    m_process = nullptr;

    if (exitCode == 0) {
        emit installFinished(true, QStringLiteral("Updated to version %1. Restart the app.").arg(m_latestVersion.toString()));
        QFile::remove(m_downloadedPath);
    } else {
        emit installFinished(false, err.isEmpty() ? QStringLiteral("Install failed") : err);
    }
}

void SelfUpdater::startInstall(const QString &pwd)
{
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SelfUpdater::onInstallFinished);

    QStringList args;
    args << QStringLiteral("-S") << QStringLiteral("zypper")
         << QStringLiteral("install") << QStringLiteral("--no-confirm")
         << m_downloadedPath;

    m_process->start(QStringLiteral("sudo"), args);
    m_process->write(pwd.toUtf8() + "\n");
    m_process->closeWriteChannel();
}

void SelfUpdater::downloadAndInstall()
{
    if (m_latestDownloadUrl.isEmpty()) {
        emit installFinished(false, QStringLiteral("No download URL available"));
        return;
    }
    m_downloadedPath = QDir::tempPath() + QStringLiteral("/opensuse-update-applet-") +
                       m_latestVersion.toString() + QStringLiteral(".rpm");
    startDownload(m_latestDownloadUrl);
}

void SelfUpdater::providePassword(const QString &pwd)
{
    if (pwd.isEmpty())
        return;
    startInstall(pwd);
}

void SelfUpdater::skipVersion()
{
    m_latestVersion = m_currentVersion;
    emit updateNotAvailable();
}

QVersionNumber SelfUpdater::currentVersion() const
{
    return m_currentVersion;
}

QVersionNumber SelfUpdater::latestVersion() const
{
    return m_latestVersion;
}

QString SelfUpdater::latestDownloadUrl() const
{
    return m_latestDownloadUrl;
}

bool SelfUpdater::isChecking() const
{
    return m_checking;
}

bool SelfUpdater::isUpdateAvailable() const
{
    return !m_latestVersion.isNull() && m_latestVersion > m_currentVersion;
}
