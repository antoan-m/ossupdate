#include "settings.h"

Settings *Settings::s_instance = nullptr;

Settings *Settings::instance()
{
    if (!s_instance)
        s_instance = new Settings();
    return s_instance;
}

Settings::Settings()
    : m_settings(QStringLiteral("OpenSUSE"), QStringLiteral("UpdateApplet"))
{
}

int Settings::checkIntervalMinutes() const
{
    return m_settings.value(QStringLiteral("check_interval_minutes"), 360).toInt();
}

void Settings::setCheckIntervalMinutes(int minutes)
{
    if (minutes != checkIntervalMinutes()) {
        m_settings.setValue(QStringLiteral("check_interval_minutes"), minutes);
        emit checkIntervalChanged(minutes);
    }
}

bool Settings::autoStart() const
{
    return m_settings.value(QStringLiteral("auto_start"), false).toBool();
}

void Settings::setAutoStart(bool enabled)
{
    if (enabled != autoStart()) {
        m_settings.setValue(QStringLiteral("auto_start"), enabled);
        emit autoStartChanged(enabled);
    }
}

bool Settings::autoUpdate() const
{
    return m_settings.value(QStringLiteral("auto_update"), false).toBool();
}

void Settings::setAutoUpdate(bool enabled)
{
    if (enabled != autoUpdate()) {
        m_settings.setValue(QStringLiteral("auto_update"), enabled);
        emit autoUpdateChanged(enabled);
    }
}

QString Settings::lastCheckTime() const
{
    return m_settings.value(QStringLiteral("last_check_time"), QString()).toString();
}

void Settings::setLastCheckTime(const QString &time)
{
    m_settings.setValue(QStringLiteral("last_check_time"), time);
}

bool Settings::useSymbolicIcons() const
{
    return m_settings.value(QStringLiteral("symbolic_icons"), false).toBool();
}

void Settings::setUseSymbolicIcons(bool symbolic)
{
    if (symbolic != useSymbolicIcons()) {
        m_settings.setValue(QStringLiteral("symbolic_icons"), symbolic);
        emit iconStyleChanged(symbolic);
    }
}
