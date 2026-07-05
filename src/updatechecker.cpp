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
    , m_expectedTotal(0)
    , m_completedCount(0)
    , m_installPhase(PhaseNone)
    , m_cachedSudoPassword()
{
    connect(m_timer, &QTimer::timeout, this, &UpdateChecker::checkNow);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
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
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray data = m_process->readAllStandardOutput();
        m_installBuffer.append(QString::fromUtf8(data));

        static QRegularExpression pkgCountRx(
            QStringLiteral(R"((\d+)\s+packages?\s+to\s+(upgrade|install))"));
        static QRegularExpression progressRx(
            QStringLiteral(R"(^\s*\(?(\d+)/(\d+)\)?\s+(Installing|Downloading|Retrieving)[:\s]+(\S+))"));
        static QRegularExpression doneRx(QStringLiteral(R"(\.*\[done\])"));

        QStringList lines = m_installBuffer.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                                   Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            auto pkgMatch = pkgCountRx.match(line);
            if (pkgMatch.hasMatch()) {
                m_expectedTotal = pkgMatch.captured(1).toInt();
                emit installOutput(QStringLiteral("Updating %1 packages...").arg(m_expectedTotal));
                continue;
            }

            auto prMatch = progressRx.match(line);
            if (prMatch.hasMatch()) {
                int current = prMatch.captured(1).toInt();
                int total = prMatch.captured(2).toInt();
                QString action = prMatch.captured(3);
                QString pkg = prMatch.captured(4);
                pkg = pkg.section(QChar('/'), 0, 0).section(QChar(':'), 0, 0);

                if (total > 0 && total <= 500) {
                    int percent = (current * 100) / total;
                    emit installProgress(percent);
                }
                emit installOutput(QStringLiteral("[%1/%2] %3: %4")
                                       .arg(current).arg(total).arg(action).arg(pkg));
                continue;
            }

            if (doneRx.match(line).hasMatch() && m_expectedTotal > 0) {
                m_completedCount++;
                int percent = (m_completedCount * 100) / m_expectedTotal;
                if (percent > 100) percent = 100;
                emit installProgress(percent);
            }
        }

        int lastNewline = m_installBuffer.lastIndexOf(QRegularExpression(QStringLiteral("[\r\n]")));
        if (lastNewline >= 0)
            m_installBuffer = m_installBuffer.mid(lastNewline + 1);
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
             << QStringLiteral("list-updates"));
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
    m_installPhase = PhaseZypper;
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
    m_installPhase = PhaseZypper;
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
    if (m_installPhase == PhaseZypper)
        doInstall(password, m_pendingInstallPackages);
    else
        doSnapInstall(password);
}

void UpdateChecker::doInstall(const QString &pwd, const QStringList &pkgs)
{
    m_installing = true;
    m_installBuffer.clear();
    m_expectedTotal = totalCount();
    m_completedCount = 0;
    m_installPhase = PhaseZypper;
    m_cachedSudoPassword = pwd;
    emit installProgress(0);
    emit installOutput(QStringLiteral("Starting zypper updates..."));

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

void UpdateChecker::doFlatpakInstall()
{
    m_installing = true;
    m_installPhase = PhaseFlatpak;
    emit installOutput(QStringLiteral("Updating Flatpak packages..."));

    QStringList args;
    args << QStringLiteral("-S") << QStringLiteral("flatpak")
         << QStringLiteral("update") << QStringLiteral("-y");
    m_process->start(QStringLiteral("sudo"), args);
    m_process->write(m_cachedSudoPassword.toUtf8() + "\n");
    m_process->closeWriteChannel();
}

void UpdateChecker::doSnapInstall(const QString &pwd)
{
    m_installing = true;
    m_installPhase = PhaseSnap;
    emit installOutput(QStringLiteral("Updating Snap packages..."));

    QStringList args;
    args << QStringLiteral("-S") << QStringLiteral("snap") << QStringLiteral("refresh");
    m_process->start(QStringLiteral("sudo"), args);
    m_process->write(pwd.toUtf8() + "\n");
    m_process->closeWriteChannel();
}

void UpdateChecker::finishAllInstalls(bool success, const QString &message)
{
    m_installing = false;
    m_installPhase = PhaseNone;
    m_cachedSudoPassword.clear();

    if (success) {
        m_zypperUpdates.clear();
        m_flatpakUpdates.clear();
        m_snapUpdates.clear();
        m_allUpdates.clear();
    }

    emit installFinished(success, message);
    if (success)
        checkNow();
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
    int ec = m_process->exitCode();
    bool success = (ec == 0);
    QString err = m_installBuffer.trimmed();

    switch (m_installPhase) {
    case PhaseZypper:
        if (ec == 4 || ec == 5 || ec == 6 || ec == 7 || ec == 102 || ec == 103)
            success = true;
        if (!success) {
            QString msg;
            QStringList lines = err.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                           Qt::SkipEmptyParts);
            for (const QString &l : lines) {
                if (l.contains(QStringLiteral("error"), Qt::CaseInsensitive)
                    || l.contains(QStringLiteral("failed"), Qt::CaseInsensitive)) {
                    msg = l.trimmed();
                    break;
                }
            }
            if (msg.isEmpty())
                msg = QStringLiteral("Zypper install failed (exit code %1)").arg(ec);
            finishAllInstalls(false, msg);
            return;
        }
        if (ec == 4 || ec == 5 || ec == 102)
            emit installOutput(QStringLiteral("Zypper updates done (reboot may be required)"));
        else
            emit installOutput(QStringLiteral("Zypper updates done"));
        if (!m_flatpakUpdates.isEmpty()) {
            doFlatpakInstall();
            return;
        }
        if (!m_snapUpdates.isEmpty()) {
            doSnapInstall(m_cachedSudoPassword);
            return;
        }
        finishAllInstalls(true, QStringLiteral("All updates installed"));
        break;

    case PhaseFlatpak:
        if (!success) {
            emit installOutput(QStringLiteral("Flatpak update failed, continuing..."));
        } else {
            emit installOutput(QStringLiteral("Flatpak updates done"));
        }
        if (!m_snapUpdates.isEmpty()) {
            doSnapInstall(m_cachedSudoPassword);
            return;
        }
        finishAllInstalls(true, QStringLiteral("All updates installed"));
        break;

    case PhaseSnap:
        if (success) {
            finishAllInstalls(true, QStringLiteral("All updates installed"));
        } else {
            finishAllInstalls(false, err.isEmpty()
                ? QStringLiteral("Snap refresh failed") : err);
        }
        break;

    default:
        finishAllInstalls(success, success
            ? QStringLiteral("Updates installed")
            : (err.isEmpty() ? QStringLiteral("Install failed") : err));
        break;
    }
}

QList<UpdateInfo> UpdateChecker::parseZypperUpdates(const QByteArray &output)
{
    QList<UpdateInfo> updates;
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")));

    for (const QString &line : lines) {
        if (!line.startsWith(QStringLiteral("v |")) && !line.startsWith(QStringLiteral("v  |")) && !line.startsWith(QStringLiteral("v|")))
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
