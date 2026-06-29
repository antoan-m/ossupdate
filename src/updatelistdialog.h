#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QList>

#include "updatechecker.h"

class LockManager;

class UpdateListDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateListDialog(const QList<UpdateInfo> &updates,
                              LockManager *lockManager,
                              QWidget *parent = nullptr);

    QStringList selectedPackages() const;

signals:
    void installRequested(const QStringList &packages);
    void lockRequested(const QString &packageName);

private slots:
    void onFilterChanged(const QString &text);
    void onSelectAll();
    void onDeselectAll();
    void onInstall();
    void onLock();

private:
    void populateTree(const QList<UpdateInfo> &updates);

    QTreeWidget *m_tree;
    QLineEdit *m_filter;
    QPushButton *m_selectAllBtn;
    QPushButton *m_deselectAllBtn;
    QPushButton *m_installBtn;
    QPushButton *m_lockBtn;
    QList<UpdateInfo> m_updates;
    LockManager *m_lockManager;
};
