#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class Settings : public QObject {
    Q_OBJECT
public:
    static Settings *instance();

    int checkIntervalMinutes() const;
    void setCheckIntervalMinutes(int minutes);

    bool autoStart() const;
    void setAutoStart(bool enabled);

    bool autoUpdate() const;
    void setAutoUpdate(bool enabled);

    QString lastCheckTime() const;
    void setLastCheckTime(const QString &time);

    bool useSymbolicIcons() const;
    void setUseSymbolicIcons(bool symbolic);

signals:
    void checkIntervalChanged(int minutes);
    void autoStartChanged(bool enabled);
    void autoUpdateChanged(bool enabled);
    void iconStyleChanged(bool symbolic);

private:
    Settings();
    QSettings m_settings;
    static Settings *s_instance;
};
