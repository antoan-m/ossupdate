#include "passwordmanager.h"

#include <qt6keychain/keychain.h>

PasswordManager::PasswordManager(QObject *parent)
    : QObject(parent)
{
}

bool PasswordManager::isCached() const
{
    return !m_cachedPassword.isEmpty();
}

void PasswordManager::clearCached()
{
    m_cachedPassword.clear();
}

void PasswordManager::savePassword(const QString &password)
{
    auto job = new QKeychain::WritePasswordJob(QStringLiteral("opensuse-update-applet"));
    job->setKey(QStringLiteral("sudo-password"));
    job->setTextData(password);
    connect(job, &QKeychain::Job::finished, this, [this, password](QKeychain::Job *j) {
        if (j->error() == QKeychain::NoError) {
            m_cachedPassword = password;
            emit passwordSaved();
        } else {
            emit errorOccurred(j->errorString());
        }
    });
    job->start();
}

void PasswordManager::getPassword(std::function<void(const QString &)> callback)
{
    if (!m_cachedPassword.isEmpty()) {
        callback(m_cachedPassword);
        return;
    }
    m_pendingCallback = callback;
    startKeychainRead();
}

void PasswordManager::deletePassword()
{
    m_cachedPassword.clear();
    auto job = new QKeychain::DeletePasswordJob(QStringLiteral("opensuse-update-applet"));
    job->setKey(QStringLiteral("sudo-password"));
    connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job *j) {
        if (j->error() == QKeychain::NoError || j->error() == QKeychain::EntryNotFound) {
            emit passwordDeleted();
        } else {
            emit errorOccurred(j->errorString());
        }
    });
    job->start();
}

void PasswordManager::startKeychainRead()
{
    auto job = new QKeychain::ReadPasswordJob(QStringLiteral("opensuse-update-applet"));
    job->setKey(QStringLiteral("sudo-password"));
    connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job *j) {
        auto cb = m_pendingCallback;
        m_pendingCallback = nullptr;
        if (j->error() == QKeychain::NoError) {
            QString pwd = static_cast<QKeychain::ReadPasswordJob *>(j)->textData();
            m_cachedPassword = pwd;
            if (cb) cb(pwd);
        } else {
            if (cb) cb(QString());
            emit passwordRequested();
        }
    });
    job->start();
}
