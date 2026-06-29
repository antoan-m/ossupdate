#include "lockmanager.h"
#include "passwordmanager.h"

#include <QProcess>
#include <QRegularExpression>

LockManager::LockManager(PasswordManager *passwordManager, QObject *parent)
    : QObject(parent)
    , m_passwordManager(passwordManager)
{
}

void LockManager::refreshLocks()
{
    QProcess proc;
    proc.start(QStringLiteral("zypper"), QStringList() << QStringLiteral("locks"));
    proc.waitForFinished(30000);
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    m_locks = parseLocksOutput(output.toUtf8());
    emit locksRefreshed(m_locks);
}

QStringList LockManager::locks() const
{
    return m_locks;
}

void LockManager::addLock(const QString &packageName)
{
    m_passwordManager->getPassword([this, packageName](const QString &pwd) {
        if (pwd.isEmpty()) {
            emit errorOccurred(QStringLiteral("No sudo password available"));
            return;
        }
        QProcess proc;
        proc.start(QStringLiteral("sudo"), QStringList()
                   << QStringLiteral("-S")
                   << QStringLiteral("zypper")
                   << QStringLiteral("addlock")
                   << packageName);
        proc.write(pwd.toUtf8() + "\n");
        proc.closeWriteChannel();
        proc.waitForFinished(30000);
        if (proc.exitCode() == 0) {
            if (!m_locks.contains(packageName))
                m_locks.append(packageName);
            emit lockAdded(packageName);
        } else {
            emit errorOccurred(QString::fromUtf8(proc.readAllStandardError()));
        }
    });
}

void LockManager::removeLock(const QString &packageName)
{
    m_passwordManager->getPassword([this, packageName](const QString &pwd) {
        if (pwd.isEmpty()) {
            emit errorOccurred(QStringLiteral("No sudo password available"));
            return;
        }
        QProcess proc;
        proc.start(QStringLiteral("sudo"), QStringList()
                   << QStringLiteral("-S")
                   << QStringLiteral("zypper")
                   << QStringLiteral("removelock")
                   << packageName);
        proc.write(pwd.toUtf8() + "\n");
        proc.closeWriteChannel();
        proc.waitForFinished(30000);
        if (proc.exitCode() == 0) {
            m_locks.removeAll(packageName);
            emit lockRemoved(packageName);
        } else {
            emit errorOccurred(QString::fromUtf8(proc.readAllStandardError()));
        }
    });
}

QStringList LockManager::parseLocksOutput(const QByteArray &output)
{
    QStringList locks;
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]")));
    for (const QString &line : lines) {
        if (line.contains(QStringLiteral("|")) && line.contains(QStringLiteral("package"))) {
            QStringList parts = line.split(QStringLiteral("|"));
            if (parts.size() >= 2) {
                QString name = parts[1].trimmed();
                if (!name.isEmpty() && !name.contains(QStringLiteral("Name")))
                    locks.append(name);
            }
        }
    }
    return locks;
}
