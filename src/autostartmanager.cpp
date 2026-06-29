#include "autostartmanager.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

AutostartManager::AutostartManager(QObject *parent)
    : QObject(parent)
{
}

bool AutostartManager::isEnabled() const
{
    return QFile::exists(desktopFilePath());
}

void AutostartManager::setEnabled(bool enabled)
{
    QString path = desktopFilePath();
    if (enabled) {
        QString execPath = resolveExecutablePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n"
                << "Type=Application\n"
                << "Name=openSUSE Update Applet\n"
                << "Comment=System tray applet for openSUSE updates\n"
                << "Exec=" << execPath << "\n"
                << "Icon=opensuse-update-applet\n"
                << "Categories=System;Utility;\n"
                << "Terminal=false\n"
                << "StartupNotify=false\n"
                << "X-GNOME-Autostart-enabled=true\n";
            file.close();
        }
    } else {
        QFile::remove(path);
    }
    emit changed(enabled);
}

QString AutostartManager::desktopFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/autostart/opensuse-update-applet.desktop");
}

QString AutostartManager::resolveExecutablePath() const
{
    QString path = QStandardPaths::findExecutable(QStringLiteral("opensuse-update-applet"));
    if (!path.isEmpty())
        return path;
    return QStringLiteral("/usr/bin/opensuse-update-applet");
}
