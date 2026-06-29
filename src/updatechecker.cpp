#include "updatechecker.h"
#include "passwordmanager.h"
#include "settings.h"

#include <QRegularExpression>
#include <QStandardPaths>

UpdateChecker::UpdateChecker(PasswordManager *passwordManager, QObject *parent)
    : QObject(parent)
    , m_passwordManager(passwordManager)
    , m_timer(new QTimer(this))
    , m_process(new QProcess(this))
    , m_pendingChecks(0)
    , m_checking(false)
    , m_installing(false)
    , m_waitingForPassword(false)
{
    connect(m_timer, &QTimer::timeout, this, &UpdateChecker::checkNow);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        Q_UNUSED(exitCode);
        Q_UNUSED(status);
        onInstallProcessFinished();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this]() {
        m_installing = false;
        emit installFinished(false, QStringLiteral("Install process failed to start"));
    });
}

void UpdateChecker::start()
{
    int interval = Settings::instance()->checkIntervalMinutes();
    m_timer->setInterval(interval * 60000);
    if (interval > 0)
        m_timer->start();
    checkNow();
}

void UpdateChecker::checkNow()
{
    if (m_checking || m_installing)
        return;

    m_checking = true;
    m_zypperUpdates.clear();
    m_flatpakUpdates.clear();
    m_snapUpdates.clear();
    m_allUpdates.clear();
    m_pendingChecks = 0;

    emit checkStarted();

    runZypperListUpdates();
    runFlatpakListUpdates();
    runSnapListUpdates();
}

void UpdateChecker::runZypperListUpdates()
{
    m_pendingChecks++;
    QProcess *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            m_zypperUpdates = parseZypperUpdates(p->readAllStandardOutput());
        }
        p->deleteLater();
        onZypperListFinished();
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            p->deleteLater();
            onZypperListFinished();
        }
    });
    p->start(QStringLiteral("zypper"), QStringList()
             << QStringLiteral("list-updates")
             << QStringLiteral("--best-effort"));
}

void UpdateChecker::runFlatpakListUpdates()
{
    m_pendingChecks++;
    if (QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty()) {
        onFlatpakListFinished();
        return;
    }
    QProcess *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            m_flatpakUpdates = parseFlatpakUpdates(p->readAllStandardOutput());
        }
        p->deleteLater();
        onFlatpakListFinished();
    });
    p->start(QStringLiteral("flatpak"), QStringList()
             << QStringLiteral("remote-ls")
             << QStringLiteral("--updates"));
}

void UpdateChecker::runSnapListUpdates()
{
    m_pendingChecks++;
    if (QStandardPaths::findExecutable(QStringLiteral("snap")).isEmpty()) {
        onSnapListFinished();
        return;
    }
    QProcess *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            m_snapUpdates = parseSnapUpdates(p->readAllStandardOutput());
        }
        p->deleteLater();
        onSnapListFinished();
    });
    p->start(QStringLiteral("snap"), QStringList()
             << QStringLiteral("refresh")
             << QStringLiteral("--list"));
}

void UpdateChecker::onZypperListFinished()
{
    m_pendingChecks--;
    allChecksDone();
}

void UpdateChecker::onFlatpakListFinished()
{
    m_pendingChecks--;
    allChecksDone();
}

void UpdateChecker::onSnapListFinished()
{
    m_pendingChecks--;
    allChecksDone();
}

void UpdateChecker::allChecksDone()
{
    if (m_pendingChecks > 0)
        return;

    m_checking = false;
    m_lastCheckTime = QDateTime::currentDateTime();
    Settings::instance()->setLastCheckTime(m_lastCheckTime.toString(Qt::ISODate));

    m_allUpdates.clear();
    m_allUpdates.append(m_zypperUpdates);
    m_allUpdates.append(m_flatpakUpdates);
    m_allUpdates.append(m_snapUpdates);

    emit updatesFound(m_allUpdates);
    emit checkFinished(true);

    if (Settings::instance()->autoUpdate() && totalCount() > 0) {
        installAll();
    }
}

void UpdateChecker::installAll()
{
    if (m_installing || m_waitingForPassword)
        return;
    m_pendingInstallPackages.clear();
    m_passwordManager->getPassword([this](const QString &pwd) {
        if (pwd.isEmpty()) {
            m_waitingForPassword = true;
            emit passwordRequired();
            return;
        }
        doInstall(pwd);
    });
}

void UpdateChecker::installSelected(const QStringList &packageNames)
{
    if (m_installing || m_waitingForPassword)
        return;
    m_pendingInstallPackages = packageNames;
    m_passwordManager->getPassword([this](const QString &pwd) {
        if (pwd.isEmpty()) {
            m_waitingForPassword = true;
            emit passwordRequired();
            return;
        }
        doInstall(pwd, m_pendingInstallPackages);
    });
}

void UpdateChecker::providePassword(const QString &password)
{
    if (!m_waitingForPassword)
        return;
    m_waitingForPassword = false;
    if (password.isEmpty())
        return;
    doInstall(password, m_pendingInstallPackages);
}

void UpdateChecker::doInstall(const QString &pwd, const QStringList &pkgs)
{
    m_installing = true;
    emit installProgress(0);
    emit installOutput(QStringLiteral("Starting installation..."));

    QStringList args;
    args << QStringLiteral("-S") << QStringLiteral("zypper");

    if (pkgs.isEmpty()) {
        args << QStringLiteral("dup") << QStringLiteral("--no-confirm");
    } else {
        args << QStringLiteral("install") << QStringLiteral("--no-confirm");
        for (const QString &pkg : pkgs)
            args << pkg;
    }

    m_process->start(QStringLiteral("sudo"), args);
    m_process->write(pwd.toUtf8() + "\n");
    m_process->closeWriteChannel();
}

void UpdateChecker::installFlatpakUpdates()
{
    if (m_installing)
        return;
    if (QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty()) {
        emit installFinished(false, QStringLiteral("flatpak command not found"));
        return;
    }
    m_installing = true;
    emit installProgress(0);
    emit installOutput(QStringLiteral("Updating Flatpak packages..."));

    QProcess *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int exitCode, QProcess::ExitStatus) {
        p->deleteLater();
        m_installing = false;
        if (exitCode == 0) {
            emit installFinished(true, QStringLiteral("Flatpak updates installed successfully"));
        } else {
            emit installFinished(false, QString::fromUtf8(p->readAllStandardError()));
        }
    });
    connect(p, &QProcess::errorOccurred, this, [this, p]() {
        p->deleteLater();
        m_installing = false;
        emit installFinished(false, QStringLiteral("flatpak failed to start"));
    });
    p->start(QStringLiteral("flatpak"), QStringList()
             << QStringLiteral("update") << QStringLiteral("-y"));
}

void UpdateChecker::installSnapUpdates()
{
    if (m_installing)
        return;
    if (QStandardPaths::findExecutable(QStringLiteral("snap")).isEmpty()) {
        emit installFinished(false, QStringLiteral("snap command not found"));
        return;
    }
    m_pendingInstallPackages.clear();
    m_passwordManager->getPassword([this](const QString &pwd) {
        if (pwd.isEmpty()) {
            emit passwordRequired();
            return;
        }
        m_installing = true;
        emit installProgress(0);
        emit installOutput(QStringLiteral("Updating Snap packages..."));

        m_process->start(QStringLiteral("sudo"), QStringList()
                         << QStringLiteral("-S")
                         << QStringLiteral("snap") << QStringLiteral("refresh"));
        m_process->write(pwd.toUtf8() + "\n");
        m_process->closeWriteChannel();
    });
}

void UpdateChecker::onInstallProcessFinished()
{
    m_installing = false;
    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    QString err = QString::fromUtf8(m_process->readAllStandardError());

    if (m_process->exitCode() == 0) {
        emit installFinished(true, QStringLiteral("Updates installed successfully"));
        checkNow();
    } else {
        emit installFinished(false, err.isEmpty() ? output : err);
    }
}

QList<UpdateInfo> UpdateChecker::parseZypperUpdates(const QByteArray &output)
{
    QList<UpdateInfo> updates;
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")));

    for (const QString &line : lines) {
        if (!line.startsWith(QStringLiteral("v |")) && !line.startsWith(QStringLiteral("v|")))
            continue;
        QStringList parts = line.split(QStringLiteral("|"));
        if (parts.size() < 6)
            continue;

        QString repo = parts[1].trimmed();
        QString name = parts[2].trimmed();
        QString oldVer = parts[3].trimmed();
        QString newVer = parts[4].trimmed();

        if (name.isEmpty() || name == QStringLiteral("Name"))
            continue;

        UpdateInfo info;
        info.name = name;
        info.repository = repo;
        info.oldVersion = oldVer;
        info.newVersion = newVer;
        info.type = QStringLiteral("zypper");
        updates.append(info);
    }
    return updates;
}

QList<UpdateInfo> UpdateChecker::parseFlatpakUpdates(const QByteArray &output)
{
    QList<UpdateInfo> updates;
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")));

    bool header = true;
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty())
            continue;
        if (header && (line.contains(QStringLiteral("Application")) || line.contains(QStringLiteral("Ref")))) {
            header = false;
            continue;
        }
        if (header) {
            header = false;
            continue;
        }

        QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
        if (parts.size() < 2)
            continue;

        UpdateInfo info;
        info.name = parts[0];
        info.type = QStringLiteral("flatpak");
        if (parts.size() >= 3)
            info.newVersion = parts[parts.size() - 1];
        if (parts.size() >= 4)
            info.repository = parts[parts.size() - 2];
        updates.append(info);
    }
    return updates;
}

QList<UpdateInfo> UpdateChecker::parseSnapUpdates(const QByteArray &output)
{
    QList<UpdateInfo> updates;
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")));

    bool header = true;
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty())
            continue;
        if (header && line.contains(QStringLiteral("Name"))) {
            header = false;
            continue;
        }
        if (header) {
            header = false;
            continue;
        }

        QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        UpdateInfo info;
        info.name = parts[0];
        info.type = QStringLiteral("snap");
        if (parts.size() >= 2)
            info.newVersion = parts[1];
        updates.append(info);
    }
    return updates;
}

QList<UpdateInfo> UpdateChecker::updates() const
{
    return m_allUpdates;
}

int UpdateChecker::zypperCount() const
{
    return m_zypperUpdates.size();
}

int UpdateChecker::flatpakCount() const
{
    return m_flatpakUpdates.size();
}

int UpdateChecker::snapCount() const
{
    return m_snapUpdates.size();
}

int UpdateChecker::totalCount() const
{
    return m_allUpdates.size();
}

bool UpdateChecker::isChecking() const
{
    return m_checking;
}

bool UpdateChecker::isInstalling() const
{
    return m_installing;
}

QDateTime UpdateChecker::lastCheckTime() const
{
    return m_lastCheckTime;
}

void UpdateChecker::setCheckInterval(int minutes)
{
    if (minutes > 0) {
        m_timer->setInterval(minutes * 60000);
        m_timer->start();
    } else {
        m_timer->stop();
    }
}
