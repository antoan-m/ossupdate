#include "mainwindow.h"
#include "updatechecker.h"
#include "lockmanager.h"
#include "passwordmanager.h"
#include "autostartmanager.h"
#include "selfupdater.h"
#include "settings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QFrame>
#include <QDateTime>
#include <QCloseEvent>
#include <QTimer>
#include <QHeaderView>
#include <QPainter>
#include <algorithm>
#include <QPainterPath>
#include <QGuiApplication>

MainWindow::MainWindow(UpdateChecker *checker,
                       LockManager *lockManager,
                       PasswordManager *passwordManager,
                       AutostartManager *autostartManager,
                       SelfUpdater *selfUpdater,
                       QWidget *parent)
    : QWidget(parent)
    , m_checker(checker)
    , m_lockManager(lockManager)
    , m_passwordManager(passwordManager)
    , m_autostartManager(autostartManager)
    , m_selfUpdater(selfUpdater)
{
    setWindowTitle(QStringLiteral("openSUSE Update Manager"));
    setMinimumSize(770, 690);
    resize(770, 690);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget();
    m_tabs->addTab(createHomeTab(), QStringLiteral("Home"));
    m_tabs->addTab(createSettingsTab(), QStringLiteral("Settings"));
    m_tabs->addTab(createLocksTab(), QStringLiteral("Locked Packages"));
    m_tabs->addTab(createAboutTab(), QStringLiteral("About"));
    mainLayout->addWidget(m_tabs);

    connect(m_checker, &UpdateChecker::checkFinished, this, &MainWindow::onCheckFinished);
    connect(m_checker, &UpdateChecker::updatesFound, this, &MainWindow::onUpdatesFound);
    connect(m_checker, &UpdateChecker::installFinished, this, &MainWindow::onInstallFinished);
    connect(m_checker, &UpdateChecker::installProgress, this, &MainWindow::onInstallProgress);
    connect(m_checker, &UpdateChecker::installOutput, this, &MainWindow::onInstallOutput);
    connect(m_checker, &UpdateChecker::passwordRequired, this, &MainWindow::onPasswordRequired);
    connect(m_selfUpdater, &SelfUpdater::updateAvailable, this, &MainWindow::onSelfUpdateAvailable);
    connect(m_selfUpdater, &SelfUpdater::updateNotAvailable, this, [this]() {
        m_versionStatusLabel->setText(
            QStringLiteral("Current: %1 (up to date)").arg(QStringLiteral(APP_VERSION)));
    });
    connect(m_selfUpdater, &SelfUpdater::installFinished, this, [this](bool success, const QString &msg) {
        if (success) {
            QMessageBox::information(this, QStringLiteral("Update Installed"), msg);
        } else {
            QMessageBox::warning(this, QStringLiteral("Update Failed"), msg);
        }
    });
    connect(m_selfUpdater, &SelfUpdater::checkFinished, this, [this](bool success) {
        m_checkUpdateBtn->setEnabled(true);
        if (!success) {
            m_versionStatusLabel->setText(
                QStringLiteral("Current: %1 (check failed)").arg(QStringLiteral(APP_VERSION)));
        }
    });
    connect(m_selfUpdater, &SelfUpdater::passwordRequired, this, [this]() {
        bool ok;
        QString pwd = QInputDialog::getText(this, QStringLiteral("Sudo Password Required"),
                                             QStringLiteral("Enter sudo password to install app update:"),
                                             QLineEdit::Password, QString(), &ok);
        if (ok && !pwd.isEmpty())
            m_selfUpdater->providePassword(pwd);
        else
            m_selfUpdater->providePassword(QString());
    });

    QTimer::singleShot(0, this, [this]() {
        connect(m_checkUpdateBtn, &QPushButton::clicked, this, &MainWindow::onCheckSelfUpdate);
        connect(m_installUpdateBtn, &QPushButton::clicked, this, &MainWindow::onSelfUpdateInstall);
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}
static QIcon paintCheckIcon(bool checked)
{
    int s = 20;
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = QGuiApplication::palette().color(QPalette::WindowText);
    p.setPen(QPen(c, 1.5));
    p.drawRoundedRect(2, 2, s - 4, s - 4, 3, 3);
    if (checked) {
        p.setPen(QPen(c, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        path.moveTo(5, s / 2);
        path.lineTo(s / 2 - 1, s - 6);
        path.lineTo(s - 4, 5);
        p.drawPath(path);
    }
    p.end();
    return QIcon(pm);
}

QWidget *MainWindow::createHomeTab()
{
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto *headerLabel = new QLabel(QStringLiteral("System Updates"));
    headerLabel->setObjectName(QStringLiteral("heading"));
    layout->addWidget(headerLabel);

    auto *cardWidget = new QWidget();
    cardWidget->setObjectName(QStringLiteral("card"));
    auto *cardLayout = new QHBoxLayout(cardWidget);
    cardLayout->setSpacing(32);

    auto *zypperBox = new QWidget();
    auto *zypperLayout = new QVBoxLayout(zypperBox);
    zypperLayout->setAlignment(Qt::AlignCenter);
    m_zypperCountLabel = new QLabel(QStringLiteral("--"));
    m_zypperCountLabel->setObjectName(QStringLiteral("count"));
    m_zypperCountLabel->setAlignment(Qt::AlignCenter);
    auto *zypperLabel = new QLabel(QStringLiteral("Zypper"));
    zypperLabel->setObjectName(QStringLiteral("subheading"));
    zypperLabel->setAlignment(Qt::AlignCenter);
    zypperLayout->addWidget(m_zypperCountLabel);
    zypperLayout->addWidget(zypperLabel);
    cardLayout->addWidget(zypperBox);

    auto *flatpakBox = new QWidget();
    auto *flatpakLayout = new QVBoxLayout(flatpakBox);
    flatpakLayout->setAlignment(Qt::AlignCenter);
    m_flatpakCountLabel = new QLabel(QStringLiteral("--"));
    m_flatpakCountLabel->setObjectName(QStringLiteral("count"));
    m_flatpakCountLabel->setAlignment(Qt::AlignCenter);
    auto *flatpakLabel = new QLabel(QStringLiteral("Flatpak"));
    flatpakLabel->setObjectName(QStringLiteral("subheading"));
    flatpakLabel->setAlignment(Qt::AlignCenter);
    flatpakLayout->addWidget(m_flatpakCountLabel);
    flatpakLayout->addWidget(flatpakLabel);
    cardLayout->addWidget(flatpakBox);

    auto *snapBox = new QWidget();
    auto *snapLayout = new QVBoxLayout(snapBox);
    snapLayout->setAlignment(Qt::AlignCenter);
    m_snapCountLabel = new QLabel(QStringLiteral("--"));
    m_snapCountLabel->setObjectName(QStringLiteral("count"));
    m_snapCountLabel->setAlignment(Qt::AlignCenter);
    auto *snapLabel = new QLabel(QStringLiteral("Snap"));
    snapLabel->setObjectName(QStringLiteral("subheading"));
    snapLabel->setAlignment(Qt::AlignCenter);
    snapLayout->addWidget(m_snapCountLabel);
    snapLayout->addWidget(snapLabel);
    cardLayout->addWidget(snapBox);

    layout->addWidget(cardWidget);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    m_selectAllBtn = new QPushButton();
    m_selectAllBtn->setIcon(paintCheckIcon(true));
    m_selectAllBtn->setIconSize(QSize(18, 18));
    m_selectAllBtn->setFixedSize(30, 30);
    m_selectAllBtn->setToolTip(QStringLiteral("Select all"));
    btnRow->addWidget(m_selectAllBtn);

    m_deselectAllBtn = new QPushButton();
    m_deselectAllBtn->setIcon(paintCheckIcon(false));
    m_deselectAllBtn->setIconSize(QSize(18, 18));
    m_deselectAllBtn->setFixedSize(30, 30);
    m_deselectAllBtn->setToolTip(QStringLiteral("Deselect all"));
    btnRow->addWidget(m_deselectAllBtn);

    btnRow->addSpacing(8);

    m_checkNowBtn = new QPushButton(QStringLiteral("Check Now"));
    btnRow->addWidget(m_checkNowBtn);

    m_installAllBtn = new QPushButton(QStringLiteral("Install All"));
    btnRow->addWidget(m_installAllBtn);

    m_installSelectedBtn = new QPushButton(QStringLiteral("Install Selected"));
    btnRow->addWidget(m_installSelectedBtn);

    m_lockSelectedBtn = new QPushButton(QStringLiteral("Lock Selected"));
    m_lockSelectedBtn->setObjectName(QStringLiteral("secondary"));
    btnRow->addWidget(m_lockSelectedBtn);

    btnRow->addStretch();
    layout->addLayout(btnRow);

    m_updateTree = new QTreeWidget();
    m_updateTree->setColumnCount(5);
    m_updateTree->setHeaderLabels({
        QStringLiteral("Package"),
        QStringLiteral("Repository"),
        QStringLiteral("Current Version"),
        QStringLiteral("New Version"),
        QStringLiteral("Type")
    });
    m_updateTree->setRootIsDecorated(false);
    m_updateTree->setAlternatingRowColors(true);
    m_updateTree->setSortingEnabled(false);
    m_updateTree->setMinimumHeight(120);
    m_updateTree->header()->setStretchLastSection(false);
    m_updateTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_updateTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_updateTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_updateTree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_updateTree->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    layout->addWidget(m_updateTree, 1);

    auto *statusGroup = new QGroupBox(QStringLiteral("Status"));
    auto *statusLayout = new QVBoxLayout(statusGroup);

    m_lastCheckLabel = new QLabel(QStringLiteral("Last check: never"));
    statusLayout->addWidget(m_lastCheckLabel);

    m_statusLabel = new QLabel(QStringLiteral("Ready"));
    statusLayout->addWidget(m_statusLabel);

    m_progressLabel = new QLabel();
    m_progressLabel->setVisible(false);
    statusLayout->addWidget(m_progressLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 0);
    statusLayout->addWidget(m_progressBar);

    layout->addWidget(statusGroup);

    connect(m_checkNowBtn, &QPushButton::clicked, this, &MainWindow::onCheckNow);
    connect(m_installAllBtn, &QPushButton::clicked, this, &MainWindow::onInstallAll);
    connect(m_installSelectedBtn, &QPushButton::clicked, this, &MainWindow::onInstallSelected);
    connect(m_lockSelectedBtn, &QPushButton::clicked, this, &MainWindow::onLockSelected);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &MainWindow::onSelectAll);
    connect(m_deselectAllBtn, &QPushButton::clicked, this, &MainWindow::onDeselectAll);

    return widget;
}

QWidget *MainWindow::createSettingsTab()
{
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *headerLabel = new QLabel(QStringLiteral("Settings"));
    headerLabel->setObjectName(QStringLiteral("heading"));
    layout->addWidget(headerLabel);

    auto *checkGroup = new QGroupBox(QStringLiteral("Update Checking"));
    auto *checkLayout = new QVBoxLayout(checkGroup);

    auto *intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new QLabel(QStringLiteral("Check interval:")));
    m_intervalCombo = new QComboBox();
    m_intervalCombo->addItem(QStringLiteral("Manual only"), 0);
    m_intervalCombo->addItem(QStringLiteral("Every 15 minutes"), 15);
    m_intervalCombo->addItem(QStringLiteral("Every 30 minutes"), 30);
    m_intervalCombo->addItem(QStringLiteral("Every hour"), 60);
    m_intervalCombo->addItem(QStringLiteral("Every 2 hours"), 120);
    m_intervalCombo->addItem(QStringLiteral("Every 6 hours"), 360);
    m_intervalCombo->addItem(QStringLiteral("Every 12 hours"), 720);
    m_intervalCombo->addItem(QStringLiteral("Every 24 hours"), 1440);

    int curInterval = Settings::instance()->checkIntervalMinutes();
    int idx = m_intervalCombo->findData(curInterval);
    if (idx >= 0)
        m_intervalCombo->setCurrentIndex(idx);

    intervalRow->addWidget(m_intervalCombo);
    intervalRow->addStretch();
    checkLayout->addLayout(intervalRow);

    m_autoUpdateCheck = new QCheckBox(QStringLiteral("Auto-apply system updates silently (no confirmation)"));
    m_autoUpdateCheck->setChecked(Settings::instance()->autoUpdate());
    checkLayout->addWidget(m_autoUpdateCheck);

    m_autoUpdateAppCheck = new QCheckBox(QStringLiteral("Auto-update this app when new version released"));
    m_autoUpdateAppCheck->setChecked(Settings::instance()->autoUpdateApp());
    checkLayout->addWidget(m_autoUpdateAppCheck);

    layout->addWidget(checkGroup);

    auto *startupGroup = new QGroupBox(QStringLiteral("Startup"));
    auto *startupLayout = new QVBoxLayout(startupGroup);

    m_autoStartCheck = new QCheckBox(QStringLiteral("Start on boot"));
    m_autoStartCheck->setChecked(m_autostartManager->isEnabled());
    startupLayout->addWidget(m_autoStartCheck);

    layout->addWidget(startupGroup);

    auto *appearanceGroup = new QGroupBox(QStringLiteral("Appearance"));
    auto *appearanceLayout = new QVBoxLayout(appearanceGroup);

    auto *iconRow = new QHBoxLayout();
    iconRow->addWidget(new QLabel(QStringLiteral("Tray icon style:")));
    m_iconStyleCombo = new QComboBox();
    m_iconStyleCombo->addItem(QStringLiteral("Colored"), false);
    m_iconStyleCombo->addItem(QStringLiteral("Symbolic"), true);
    m_iconStyleCombo->setCurrentIndex(Settings::instance()->useSymbolicIcons() ? 1 : 0);
    iconRow->addWidget(m_iconStyleCombo);
    iconRow->addStretch();
    appearanceLayout->addLayout(iconRow);

    layout->addWidget(appearanceGroup);

    auto *pwdGroup = new QGroupBox(QStringLiteral("Sudo Password"));
    auto *pwdLayout = new QVBoxLayout(pwdGroup);

    auto *pwdLabel = new QLabel(
        QStringLiteral("Store sudo password in system keychain to avoid password prompts when installing updates."));
    pwdLabel->setWordWrap(true);
    pwdLabel->setObjectName(QStringLiteral("subheading"));
    pwdLayout->addWidget(pwdLabel);

    auto *pwdRow = new QHBoxLayout();
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("Enter sudo password..."));
    pwdRow->addWidget(m_passwordEdit);

    m_savePasswordBtn = new QPushButton(QStringLiteral("Save"));
    pwdRow->addWidget(m_savePasswordBtn);

    m_clearPasswordBtn = new QPushButton(QStringLiteral("Clear"));
    m_clearPasswordBtn->setObjectName(QStringLiteral("danger"));
    pwdRow->addWidget(m_clearPasswordBtn);

    pwdLayout->addLayout(pwdRow);

    m_passwordStatus = new QLabel();
    m_passwordStatus->setObjectName(QStringLiteral("subheading"));
    pwdLayout->addWidget(m_passwordStatus);

    layout->addWidget(pwdGroup);
    layout->addStretch();

    connect(m_intervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onIntervalChanged);
    connect(m_autoStartCheck, &QCheckBox::toggled, this, &MainWindow::onAutoStartToggled);
    connect(m_autoUpdateCheck, &QCheckBox::toggled, this, &MainWindow::onAutoUpdateToggled);
    connect(m_autoUpdateAppCheck, &QCheckBox::toggled, this, [this](bool checked) {
        Settings::instance()->setAutoUpdateApp(checked);
    });
    connect(m_iconStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onIconStyleChanged);
    connect(m_savePasswordBtn, &QPushButton::clicked, this, &MainWindow::onSavePassword);
    connect(m_clearPasswordBtn, &QPushButton::clicked, this, &MainWindow::onClearPassword);

    updatePasswordIndicator();

    return widget;
}

QWidget *MainWindow::createLocksTab()
{
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *headerLabel = new QLabel(QStringLiteral("Locked Packages"));
    headerLabel->setObjectName(QStringLiteral("heading"));
    layout->addWidget(headerLabel);

    auto *descLabel = new QLabel(
        QStringLiteral("Locked packages will be skipped when checking for updates."));
    descLabel->setObjectName(QStringLiteral("subheading"));
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    m_locksList = new QListWidget();
    layout->addWidget(m_locksList);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    m_addLockBtn = new QPushButton(QStringLiteral("Add Lock"));
    m_addLockBtn->setObjectName(QStringLiteral("secondary"));
    btnRow->addWidget(m_addLockBtn);

    m_removeLockBtn = new QPushButton(QStringLiteral("Remove Selected"));
    m_removeLockBtn->setObjectName(QStringLiteral("danger"));
    btnRow->addWidget(m_removeLockBtn);

    btnRow->addStretch();

    m_refreshLocksBtn = new QPushButton(QStringLiteral("Refresh"));
    btnRow->addWidget(m_refreshLocksBtn);

    layout->addLayout(btnRow);

    connect(m_addLockBtn, &QPushButton::clicked, this, &MainWindow::onAddLock);
    connect(m_removeLockBtn, &QPushButton::clicked, this, &MainWindow::onRemoveLock);
    connect(m_refreshLocksBtn, &QPushButton::clicked, this, &MainWindow::onRefreshLocks);
    connect(m_lockManager, &LockManager::locksRefreshed, this, [this](const QStringList &locks) {
        m_locksList->clear();
        for (const QString &lock : locks)
            m_locksList->addItem(lock);
    });

    QTimer::singleShot(0, m_lockManager, &LockManager::refreshLocks);

    return widget;
}

void MainWindow::refreshUpdateSummary()
{
    m_zypperCountLabel->setText(QString::number(m_checker->zypperCount()));
    m_flatpakCountLabel->setText(QString::number(m_checker->flatpakCount()));
    m_snapCountLabel->setText(QString::number(m_checker->snapCount()));

    QDateTime lastCheck = m_checker->lastCheckTime();
    if (lastCheck.isValid()) {
        m_lastCheckLabel->setText(
            QStringLiteral("Last check: %1").arg(lastCheck.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    }
}

void MainWindow::onCheckNow()
{
    m_statusLabel->setText(QStringLiteral("Checking for updates..."));
    m_checker->checkNow();
}

void MainWindow::populateUpdateTree(const QList<UpdateInfo> &updates)
{
    m_updateTree->clear();
    if (updates.isEmpty()) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, QStringLiteral("All packages are up to date"));
        for (int i = 1; i < 5; ++i)
            item->setText(i, QString());
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(0, QGuiApplication::palette().color(QPalette::Disabled, QPalette::Text));
        m_updateTree->addTopLevelItem(item);
        m_selectAllBtn->setEnabled(false);
        m_deselectAllBtn->setEnabled(false);
        m_installSelectedBtn->setEnabled(false);
        m_lockSelectedBtn->setEnabled(false);
        return;
    }
    QList<UpdateInfo> sorted = updates;
    std::sort(sorted.begin(), sorted.end(), [](const UpdateInfo &a, const UpdateInfo &b) {
        return a.name.toLower() < b.name.toLower();
    });
    for (const auto &info : sorted) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, info.name);
        item->setText(1, info.repository);
        item->setText(2, info.oldVersion);
        item->setText(3, info.newVersion);
        item->setText(4, info.type);
        item->setCheckState(0, Qt::Checked);
        item->setData(0, Qt::UserRole, info.name);
        m_updateTree->addTopLevelItem(item);
    }
    m_selectAllBtn->setEnabled(true);
    m_deselectAllBtn->setEnabled(true);
    m_installSelectedBtn->setEnabled(true);
    m_lockSelectedBtn->setEnabled(true);
}

QStringList MainWindow::selectedZypperPackages() const
{
    QStringList pkgs;
    for (int i = 0; i < m_updateTree->topLevelItemCount(); ++i) {
        auto *item = m_updateTree->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked && item->text(4) == QStringLiteral("zypper"))
            pkgs.append(item->text(0));
    }
    return pkgs;
}

void MainWindow::onSelectAll()
{
    for (int i = 0; i < m_updateTree->topLevelItemCount(); ++i)
        m_updateTree->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

void MainWindow::onDeselectAll()
{
    for (int i = 0; i < m_updateTree->topLevelItemCount(); ++i)
        m_updateTree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}

void MainWindow::onInstallSelected()
{
    QStringList pkgs = selectedZypperPackages();
    if (pkgs.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Selection"),
                                 QStringLiteral("No zypper packages selected."));
        return;
    }
    m_checker->installSelected(pkgs);
}

void MainWindow::onLockSelected()
{
    QStringList pkgs = selectedZypperPackages();
    for (const QString &pkg : pkgs)
        m_lockManager->addLock(pkg);
}

void MainWindow::onInstallAll()
{
    if (m_checker->totalCount() == 0) {
        QMessageBox::information(this, QStringLiteral("No Updates"),
                                 QStringLiteral("All packages are up to date."));
        return;
    }
    m_checker->installAll();
}

void MainWindow::onCheckFinished(bool success)
{
    if (success) {
        m_statusLabel->setText(QStringLiteral("Check completed"));
        refreshUpdateSummary();
    } else {
        m_statusLabel->setText(QStringLiteral("Check failed"));
    }
    m_checkNowBtn->setEnabled(true);
}

void MainWindow::onUpdatesFound(const QList<UpdateInfo> &updates)
{
    populateUpdateTree(updates);
    refreshUpdateSummary();
}

void MainWindow::onInstallFinished(bool success, const QString &message)
{
    m_progressBar->setVisible(false);
    m_progressLabel->setVisible(false);
    m_installAllBtn->setEnabled(true);
    m_installSelectedBtn->setEnabled(true);

    if (success) {
        m_statusLabel->setText(QStringLiteral("Updates installed successfully"));
        populateUpdateTree({});
    } else {
        m_statusLabel->setText(QStringLiteral("Installation failed"));
        QMessageBox::warning(this, QStringLiteral("Installation Failed"), message);
    }
}

void MainWindow::onInstallProgress(int percent)
{
    m_progressBar->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(percent);
}

void MainWindow::onInstallOutput(const QString &line)
{
    m_progressLabel->setText(line);
    m_progressLabel->setVisible(true);
}

void MainWindow::onIntervalChanged(int index)
{
    int minutes = m_intervalCombo->itemData(index).toInt();
    Settings::instance()->setCheckIntervalMinutes(minutes);
    m_checker->setCheckInterval(minutes);
}

void MainWindow::onAutoStartToggled(bool checked)
{
    m_autostartManager->setEnabled(checked);
    Settings::instance()->setAutoStart(checked);
}

void MainWindow::onAutoUpdateToggled(bool checked)
{
    Settings::instance()->setAutoUpdate(checked);
}

void MainWindow::onSavePassword()
{
    QString password = m_passwordEdit->text();
    if (password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Password Required"),
                             QStringLiteral("Please enter a password."));
        return;
    }
    m_passwordManager->savePassword(password);
    m_passwordEdit->clear();
    updatePasswordIndicator();
}

void MainWindow::onClearPassword()
{
    m_passwordManager->deletePassword();
    updatePasswordIndicator();
}

void MainWindow::onRefreshLocks()
{
    m_lockManager->refreshLocks();
}

void MainWindow::onAddLock()
{
    bool ok;
    QString pkg = QInputDialog::getText(this, QStringLiteral("Add Lock"),
                                         QStringLiteral("Package name to lock:"),
                                         QLineEdit::Normal, QString(), &ok);
    if (ok && !pkg.isEmpty())
        m_lockManager->addLock(pkg);
}

void MainWindow::onRemoveLock()
{
    auto *item = m_locksList->currentItem();
    if (item)
        m_lockManager->removeLock(item->text());
}

void MainWindow::onIconStyleChanged(int index)
{
    bool symbolic = m_iconStyleCombo->itemData(index).toBool();
    Settings::instance()->setUseSymbolicIcons(symbolic);
}

void MainWindow::onCheckSelfUpdate()
{
    m_checkUpdateBtn->setEnabled(false);
    m_versionStatusLabel->setText(QStringLiteral("Checking for updates..."));
    m_selfUpdater->checkNow();
}

void MainWindow::onSelfUpdateAvailable(const QString &version, const QString &downloadUrl)
{
    Q_UNUSED(downloadUrl);
    m_versionStatusLabel->setText(
        QStringLiteral("Current: %1  |  Latest: %2").arg(QStringLiteral(APP_VERSION)).arg(version));
    m_installUpdateBtn->setVisible(true);

    if (Settings::instance()->autoUpdateApp())
        m_selfUpdater->downloadAndInstall();
}

void MainWindow::onSelfUpdateInstall()
{
    m_installUpdateBtn->setEnabled(false);
    m_installUpdateBtn->setText(QStringLiteral("Downloading..."));
    m_selfUpdater->downloadAndInstall();
}

void MainWindow::onPasswordRequired()
{
    bool ok;
    QString pwd = QInputDialog::getText(this, QStringLiteral("Sudo Password Required"),
                                         QStringLiteral("Enter sudo password to install updates:"),
                                         QLineEdit::Password, QString(), &ok);
    if (ok && !pwd.isEmpty()) {
        m_checker->providePassword(pwd);
    } else {
        m_checker->providePassword(QString());
    }
}

QWidget *MainWindow::createAboutTab()
{
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addStretch();

    auto *nameLabel = new QLabel(QStringLiteral("openSUSE Update Applet"));
    nameLabel->setObjectName(QStringLiteral("heading"));
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    auto *versionLabel = new QLabel(QStringLiteral("Version %1").arg(QStringLiteral(APP_VERSION)));
    versionLabel->setObjectName(QStringLiteral("subheading"));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    layout->addSpacing(8);

    auto *authorLabel = new QLabel(QStringLiteral("Author: antoan-m"));
    authorLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(authorLabel);

    layout->addSpacing(16);

    auto *repoLabel = new QLabel();
    repoLabel->setText(
        QStringLiteral("<a href='https://github.com/antoan-m/opensuse_autoupdate_applet'>"
                       "github.com/antoan-m/opensuse_autoupdate_applet</a>"));
    repoLabel->setOpenExternalLinks(true);
    repoLabel->setTextFormat(Qt::RichText);
    repoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(repoLabel);

    layout->addSpacing(24);

    auto *descLabel = new QLabel(
        QStringLiteral("A system tray applet that periodically checks for\n"
                       "openSUSE system updates (zypper/flatpak/snap)\n"
                       "and notifies you when updates are available."));
    descLabel->setObjectName(QStringLiteral("subheading"));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    layout->addSpacing(24);

    auto *licenseLabel = new QLabel(
        QStringLiteral("Licensed under the GNU General Public License v3.0"));
    licenseLabel->setObjectName(QStringLiteral("subheading"));
    licenseLabel->setAlignment(Qt::AlignCenter);
    licenseLabel->setWordWrap(true);
    layout->addWidget(licenseLabel);

    layout->addSpacing(32);

    auto *updateGroup = new QGroupBox(QStringLiteral("App Updates"));
    auto *updateLayout = new QVBoxLayout(updateGroup);

    m_versionStatusLabel = new QLabel(
        QStringLiteral("Current: %1").arg(QStringLiteral(APP_VERSION)));
    m_versionStatusLabel->setObjectName(QStringLiteral("subheading"));
    updateLayout->addWidget(m_versionStatusLabel);

    auto *updateBtnRow = new QHBoxLayout();
    m_checkUpdateBtn = new QPushButton(QStringLiteral("Check for Updates"));
    m_checkUpdateBtn->setObjectName(QStringLiteral("secondary"));
    updateBtnRow->addWidget(m_checkUpdateBtn);

    m_installUpdateBtn = new QPushButton(QStringLiteral("Download & Install Update"));
    m_installUpdateBtn->setVisible(false);
    updateBtnRow->addWidget(m_installUpdateBtn);

    updateBtnRow->addStretch();
    updateLayout->addLayout(updateBtnRow);

    layout->addWidget(updateGroup);

    layout->addStretch();
    return widget;
}

void MainWindow::updatePasswordIndicator()
{
    m_passwordManager->getPassword([this](const QString &pwd) {
        if (pwd.isEmpty()) {
            m_passwordStatus->setText(QStringLiteral("No password stored in keychain."));
            m_clearPasswordBtn->setEnabled(false);
        } else {
            m_passwordStatus->setText(QStringLiteral("Password is stored in keychain."));
            m_clearPasswordBtn->setEnabled(true);
        }
    });
}
