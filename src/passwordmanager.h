#pragma once

#include <QObject>
#include <QString>
#include <functional>

class PasswordManager : public QObject {
    Q_OBJECT
public:
    explicit PasswordManager(QObject *parent = nullptr);

    bool isCached() const;
    void clearCached();

public:
    void savePassword(const QString &password);
    void getPassword(std::function<void(const QString &)> callback);
    void deletePassword();

signals:
    void passwordSaved();
    void passwordDeleted();
    void errorOccurred(const QString &message);
    void passwordRequested();

private:
    QString m_cachedPassword;
    std::function<void(const QString &)> m_pendingCallback;

    void startKeychainRead();
};
