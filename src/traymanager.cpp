#include "traymanager.h"
#include "updatechecker.h"
#include "settings.h"

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>

TrayManager::TrayManager(UpdateChecker *checker, QWidget *mainWindow, QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
    , m_mainWindow(mainWindow)
    , m_checker(checker)
    , m_currentState(Idle)
{
    buildMenu();
    m_trayIcon->setContextMenu(m_menu);
    m_trayIcon->setToolTip(QStringLiteral("openSUSE Update Applet"));

    connect(Settings::instance(), &Settings::iconStyleChanged, this, [this](bool) {
        createIcon(m_currentState);
    });

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &TrayManager::onTrayActivated);
    connect(m_checker, &UpdateChecker::updatesFound,
            this, &TrayManager::onUpdatesFound);
    connect(m_checker, &UpdateChecker::checkStarted,
            this, &TrayManager::onCheckStarted);
    connect(m_checker, &UpdateChecker::checkFinished,
            this, &TrayManager::onCheckFinished);
}

TrayManager::~TrayManager()
{
    delete m_menu;
}

void TrayManager::show()
{
    setIconState(Idle);
    m_trayIcon->show();
}

void TrayManager::buildMenu()
{
    m_openAction = m_menu->addAction(QStringLiteral("Open Update Manager"));
    connect(m_openAction, &QAction::triggered, this, &TrayManager::showWindowRequested);

    m_checkAction = m_menu->addAction(QStringLiteral("Check Now"));
    connect(m_checkAction, &QAction::triggered, this, &TrayManager::checkNowRequested);

    m_menu->addSeparator();

    m_lastCheckAction = m_menu->addAction(QStringLiteral("Last checked: never"));
    m_lastCheckAction->setEnabled(false);

    m_menu->addSeparator();

    m_quitAction = m_menu->addAction(QStringLiteral("Quit"));
    connect(m_quitAction, &QAction::triggered, this, &TrayManager::quitRequested);
}

void TrayManager::setIconState(IconState state)
{
    m_currentState = state;
    createIcon(state);
}

static QIcon paintSymbolicIcon(const QString &shape)
{
    int size = 24;
    qreal dpr = 1.0;
    if (auto *screen = QGuiApplication::primaryScreen())
        dpr = screen->devicePixelRatio();
    QPixmap pm(size * dpr, size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QColor color = QGuiApplication::palette().color(QPalette::WindowText);
    QPen pen(color, 1.8 * dpr);
    p.setPen(pen);

    qreal cx = size / 2.0 * dpr;
    qreal cy = size / 2.0 * dpr;
    qreal r = (size / 2.0 - 1.5) * dpr;

    if (shape == QStringLiteral("idle")) {
        p.drawEllipse(QPointF(cx, cy), r, r);
        QPen checkPen(color, 2.0 * dpr, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(checkPen);
        QPainterPath path;
        path.moveTo(cx - r * 0.45, cy);
        path.lineTo(cx - r * 0.1, cy + r * 0.4);
        path.lineTo(cx + r * 0.55, cy - r * 0.35);
        p.drawPath(path);
    } else if (shape == QStringLiteral("checking")) {
        p.drawEllipse(QPointF(cx, cy), r, r);
        QPen handPen(color, 2.0 * dpr, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(handPen);
        QPainterPath path;
        path.moveTo(cx, cy - r * 0.5);
        path.lineTo(cx, cy);
        path.lineTo(cx + r * 0.35, cy + r * 0.35);
        p.drawPath(path);
    } else if (shape == QStringLiteral("updates")) {
        p.drawEllipse(QPointF(cx, cy), r, r);
        QPen exclPen(color, 2.0 * dpr, Qt::SolidLine, Qt::RoundCap);
        p.setPen(exclPen);
        p.drawLine(QPointF(cx, cy - r * 0.35), QPointF(cx, cy + r * 0.1));
        qreal dotR = 1.5 * dpr;
        p.drawEllipse(QPointF(cx, cy + r * 0.35), dotR, dotR);
    }

    p.end();
    return QIcon(pm);
}

void TrayManager::createIcon(IconState state)
{
    bool symbolic = Settings::instance()->useSymbolicIcons();

    if (symbolic) {
        QString shape;
        switch (state) {
        case Idle:              shape = QStringLiteral("idle"); break;
        case Checking:          shape = QStringLiteral("checking"); break;
        case UpdatesAvailable:  shape = QStringLiteral("updates"); break;
        }
        m_trayIcon->setIcon(paintSymbolicIcon(shape));
        return;
    }

    QString prefix = QStringLiteral(":/icons/tray_%1.svg");
    QString name;
    switch (state) {
    case Idle:              name = QStringLiteral("idle"); break;
    case Checking:          name = QStringLiteral("checking"); break;
    case UpdatesAvailable:  name = QStringLiteral("updates"); break;
    }
    m_trayIcon->setIcon(QIcon(prefix.arg(name)));
}

void TrayManager::showNotification(const QString &title, const QString &message,
                                   QSystemTrayIcon::MessageIcon icon, int durationMs)
{
    m_trayIcon->showMessage(title, message, icon, durationMs);
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger)
        emit showWindowRequested();
}

void TrayManager::onUpdatesFound(const QList<UpdateInfo> &updates)
{
    int total = updates.size();
    if (total > 0) {
        setIconState(UpdatesAvailable);

        int zypper = m_checker->zypperCount();
        int flatpak = m_checker->flatpakCount();
        int snap = m_checker->snapCount();

        QString msg;
        msg += QStringLiteral("Zypper: %1").arg(zypper);
        if (flatpak > 0)
            msg += QStringLiteral(" | Flatpak: %1").arg(flatpak);
        if (snap > 0)
            msg += QStringLiteral(" | Snap: %1").arg(snap);

        showNotification(
            QStringLiteral("Updates Available"),
            QStringLiteral("%1 updates available\n%2").arg(total).arg(msg));
    } else {
        setIconState(Idle);
    }
}

void TrayManager::onCheckStarted()
{
    m_checkAction->setEnabled(false);
    setIconState(Checking);
    m_lastCheckAction->setText(QStringLiteral("Checking..."));
}

void TrayManager::onCheckFinished(bool success)
{
    m_checkAction->setEnabled(true);
    if (m_checker->totalCount() == 0)
        setIconState(Idle);

    if (success) {
        QDateTime lastCheck = m_checker->lastCheckTime();
        if (lastCheck.isValid()) {
            m_lastCheckAction->setText(
                QStringLiteral("Last checked: %1")
                    .arg(lastCheck.toString(QStringLiteral("HH:mm:ss"))));
        }
    } else {
        m_lastCheckAction->setText(QStringLiteral("Last check failed"));
    }
}
