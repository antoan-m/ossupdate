#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QTreeWidget>

#include "updatechecker.h"

class UpdateChecker;
class LockManager;
class PasswordManager;
class AutostartManager;
class SelfUpdater;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(UpdateChecker *checker,
                        LockManager *lockManager,
                        PasswordManager *passwordManager,
                        AutostartManager *autostartManager,
                        SelfUpdater *selfUpdater,
                        QWidget *parent = nullptr);

    void refreshUpdateSummary();

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void closeRequested();

private slots:
    void onCheckNow();
    void onInstallAll();
    void onSelectAll();
    void onDeselectAll();
    void onInstallSelected();
    void onLockSelected();
    void onCheckFinished(bool success);
    void onUpdatesFound(const QList<UpdateInfo> &updates);
    void onInstallFinished(bool success, const QString &message);
    void onInstallProgress(int percent);
    void onInstallOutput(const QString &line);
    void onIntervalChanged(int index);
    void onAutoStartToggled(bool checked);
    void onAutoUpdateToggled(bool checked);
    void onSavePassword();
    void onClearPassword();
    void onIconStyleChanged(int index);
    void onRefreshLocks();
    void onAddLock();
    void onRemoveLock();
    void onPasswordRequired();
    void onCheckSelfUpdate();
    void onSelfUpdateAvailable(const QString &version, const QString &downloadUrl);
    void onSelfUpdateInstall();

private:
    QWidget *createHomeTab();
    QWidget *createSettingsTab();
    QWidget *createLocksTab();
    QWidget *createAboutTab();
    void populateUpdateTree(const QList<UpdateInfo> &updates);
    QStringList selectedZypperPackages() const;
    void updatePasswordIndicator();

    UpdateChecker *m_checker;
    LockManager *m_lockManager;
    PasswordManager *m_passwordManager;
    AutostartManager *m_autostartManager;
    SelfUpdater *m_selfUpdater;

    QTabWidget *m_tabs;

    // Home tab
    QLabel *m_zypperCountLabel;
    QLabel *m_flatpakCountLabel;
    QLabel *m_snapCountLabel;
    QLabel *m_statusLabel;
    QLabel *m_lastCheckLabel;
    QPushButton *m_checkNowBtn;
    QPushButton *m_installAllBtn;
    QPushButton *m_installSelectedBtn;
    QPushButton *m_lockSelectedBtn;
    QPushButton *m_selectAllBtn;
    QPushButton *m_deselectAllBtn;
    QTreeWidget *m_updateTree;
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    QLabel *m_rebootStatusLabel;

    // Settings tab
    QComboBox *m_intervalCombo;
    QCheckBox *m_autoStartCheck;
    QCheckBox *m_autoUpdateCheck;
    QComboBox *m_iconStyleCombo;
    QLineEdit *m_passwordEdit;
    QPushButton *m_savePasswordBtn;
    QPushButton *m_clearPasswordBtn;
    QLabel *m_passwordStatus;

    // Locks tab
    QListWidget *m_locksList;
    QPushButton *m_addLockBtn;
    QPushButton *m_removeLockBtn;
    QPushButton *m_refreshLocksBtn;

    // About tab
    QLabel *m_versionStatusLabel;
    QPushButton *m_checkUpdateBtn;
    QPushButton *m_installUpdateBtn;

    // Settings tab
    QCheckBox *m_autoUpdateAppCheck;
};
