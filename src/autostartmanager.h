#pragma once

#include <QObject>
#include <QString>

class AutostartManager : public QObject {
    Q_OBJECT
public:
    explicit AutostartManager(QObject *parent = nullptr);

    bool isEnabled() const;
    void setEnabled(bool enabled);
    QString desktopFilePath() const;

signals:
    void changed(bool enabled);

private:
    QString resolveExecutablePath() const;
};
