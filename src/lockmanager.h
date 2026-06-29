#pragma once

#include <QObject>
#include <QStringList>

class PasswordManager;

class LockManager : public QObject {
    Q_OBJECT
public:
    explicit LockManager(PasswordManager *passwordManager, QObject *parent = nullptr);

    void refreshLocks();
    QStringList locks() const;

    void addLock(const QString &packageName);
    void removeLock(const QString &packageName);

signals:
    void locksRefreshed(const QStringList &locks);
    void lockAdded(const QString &packageName);
    void lockRemoved(const QString &packageName);
    void errorOccurred(const QString &message);

private:
    QStringList parseLocksOutput(const QByteArray &output);

    PasswordManager *m_passwordManager;
    QStringList m_locks;
};
