#include "updatelistdialog.h"
#include "lockmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

UpdateListDialog::UpdateListDialog(const QList<UpdateInfo> &updates,
                                   LockManager *lockManager,
                                   QWidget *parent)
    : QDialog(parent)
    , m_updates(updates)
    , m_lockManager(lockManager)
{
    setWindowTitle(QStringLiteral("Available Updates"));
    setMinimumSize(700, 450);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    auto *headerLabel = new QLabel(QStringLiteral("Available Updates"));
    headerLabel->setObjectName(QStringLiteral("heading"));
    mainLayout->addWidget(headerLabel);

    auto *subLabel = new QLabel(
        QStringLiteral("%1 updates available").arg(updates.size()));
    subLabel->setObjectName(QStringLiteral("subheading"));
    mainLayout->addWidget(subLabel);

    m_filter = new QLineEdit();
    m_filter->setPlaceholderText(QStringLiteral("Search packages..."));
    mainLayout->addWidget(m_filter);

    m_tree = new QTreeWidget();
    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels({
        QStringLiteral("Package"),
        QStringLiteral("Repository"),
        QStringLiteral("Current Version"),
        QStringLiteral("New Version"),
        QStringLiteral("Type")
    });
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_tree);

    auto *btnLayout = new QHBoxLayout();

    m_selectAllBtn = new QPushButton(QStringLiteral("Select All"));
    m_selectAllBtn->setObjectName(QStringLiteral("secondary"));
    btnLayout->addWidget(m_selectAllBtn);

    m_deselectAllBtn = new QPushButton(QStringLiteral("Deselect All"));
    m_deselectAllBtn->setObjectName(QStringLiteral("secondary"));
    btnLayout->addWidget(m_deselectAllBtn);

    btnLayout->addStretch();

    m_lockBtn = new QPushButton(QStringLiteral("Lock Selected"));
    m_lockBtn->setObjectName(QStringLiteral("secondary"));
    btnLayout->addWidget(m_lockBtn);

    m_installBtn = new QPushButton(QStringLiteral("Install Selected"));
    btnLayout->addWidget(m_installBtn);

    mainLayout->addLayout(btnLayout);

    connect(m_filter, &QLineEdit::textChanged, this, &UpdateListDialog::onFilterChanged);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &UpdateListDialog::onSelectAll);
    connect(m_deselectAllBtn, &QPushButton::clicked, this, &UpdateListDialog::onDeselectAll);
    connect(m_installBtn, &QPushButton::clicked, this, &UpdateListDialog::onInstall);
    connect(m_lockBtn, &QPushButton::clicked, this, &UpdateListDialog::onLock);

    populateTree(m_updates);
}

void UpdateListDialog::populateTree(const QList<UpdateInfo> &updates)
{
    m_tree->clear();
    for (const auto &info : updates) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, info.name);
        item->setText(1, info.repository);
        item->setText(2, info.oldVersion);
        item->setText(3, info.newVersion);
        item->setText(4, info.type);
        item->setCheckState(0, Qt::Checked);
        item->setData(0, Qt::UserRole, info.name);
        m_tree->addTopLevelItem(item);
    }
}

QStringList UpdateListDialog::selectedPackages() const
{
    QStringList pkgs;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *item = m_tree->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            QString type = item->text(4);
            if (type == QStringLiteral("zypper"))
                pkgs.append(item->text(0));
        }
    }
    return pkgs;
}

void UpdateListDialog::onFilterChanged(const QString &text)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *item = m_tree->topLevelItem(i);
        bool match = text.isEmpty() ||
                     item->text(0).contains(text, Qt::CaseInsensitive) ||
                     item->text(1).contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void UpdateListDialog::onSelectAll()
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

void UpdateListDialog::onDeselectAll()
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}

void UpdateListDialog::onInstall()
{
    QStringList pkgs = selectedPackages();
    if (!pkgs.isEmpty())
        emit installRequested(pkgs);
    accept();
}

void UpdateListDialog::onLock()
{
    QStringList pkgs = selectedPackages();
    for (const QString &pkg : pkgs)
        emit lockRequested(pkg);
}
