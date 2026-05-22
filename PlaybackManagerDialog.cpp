#include "PlaybackManagerDialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PlaybackManagerDialog::PlaybackManagerDialog(const QString& downloadsPath,
                                             QWidget* parent)
    : QDialog(parent), m_downloadsPath(downloadsPath)
{
    setWindowTitle("Manage Playbacks");
    setMinimumSize(640, 400);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Name", "Capacity (MB)", "Created at"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 110);
    m_table->setColumnWidth(2, 160);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->hide();

    m_totalLabel = new QLabel(this);

    auto* closeBtn = new QPushButton("Close", this);
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_totalLabel);
    layout->addLayout(btnRow);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_table, &QTableWidget::cellClicked,
            this, &PlaybackManagerDialog::onCellClicked);
    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &PlaybackManagerDialog::onCustomContextMenu);

    refresh();
}

void PlaybackManagerDialog::refresh() {
    m_table->setRowCount(0);

    QDir dir(m_downloadsPath);
    QFileInfoList files = dir.entryInfoList({"*.mp4"}, QDir::Files, QDir::Time);

    double totalMB = 0.0;

    for (const QFileInfo& fi : files) {
        double mb = fi.size() / 1048576.0;
        totalMB += mb;

        int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setData(Qt::UserRole, fi.absoluteFilePath());
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        auto* sizeItem = new QTableWidgetItem(QString::number(mb, 'f', 2));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sizeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        QDateTime created = fi.birthTime().isValid()
                          ? fi.birthTime()
                          : fi.lastModified();
        auto* dateItem = new QTableWidgetItem(
            created.toString("yyyy-MM-dd HH:mm:ss"));
        dateItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, sizeItem);
        m_table->setItem(row, 2, dateItem);
    }

    // Total row (bold, non-selectable)
    int totalRow = m_table->rowCount();
    m_table->insertRow(totalRow);

    QFont bold;
    bold.setBold(true);

    auto* totalNameItem = new QTableWidgetItem("Total");
    totalNameItem->setFont(bold);
    totalNameItem->setFlags(Qt::ItemIsEnabled);

    auto* totalSizeItem = new QTableWidgetItem(
        QString::number(totalMB, 'f', 2));
    totalSizeItem->setFont(bold);
    totalSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalSizeItem->setFlags(Qt::ItemIsEnabled);

    auto* totalDateItem = new QTableWidgetItem;
    totalDateItem->setFlags(Qt::ItemIsEnabled);

    m_table->setItem(totalRow, 0, totalNameItem);
    m_table->setItem(totalRow, 1, totalSizeItem);
    m_table->setItem(totalRow, 2, totalDateItem);

    m_totalLabel->setText(QString("Total: %1 MB  (%2 file(s))")
                          .arg(totalMB, 0, 'f', 2)
                          .arg(files.size()));
}

QString PlaybackManagerDialog::filePathAt(int row) const {
    auto* item = m_table->item(row, 0);
    if (!item) return {};
    return item->data(Qt::UserRole).toString();
}

void PlaybackManagerDialog::onCellClicked(int row, int /*col*/) {
    QString path = filePathAt(row);
    if (path.isEmpty()) return;  // total row
    m_selectedPath = path;
    accept();
}

void PlaybackManagerDialog::onCustomContextMenu(const QPoint& pos) {
    QTableWidgetItem* item = m_table->itemAt(pos);
    if (!item) return;

    int row = item->row();
    // Guard: total row has no UserRole path
    if (filePathAt(row).isEmpty()) return;

    m_contextRow = row;

    QMenu menu(this);
    menu.addAction("Rename", this, &PlaybackManagerDialog::onRenameFile);
    menu.addAction("Delete", this, &PlaybackManagerDialog::onDeleteFile);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void PlaybackManagerDialog::onRenameFile() {
    if (m_contextRow < 0) return;
    QString fullPath = filePathAt(m_contextRow);
    if (fullPath.isEmpty()) return;

    QFileInfo fi(fullPath);
    QString oldBase = fi.completeBaseName();

    bool ok = false;
    QString newBase = QInputDialog::getText(this, "Rename", "New name:",
                                            QLineEdit::Normal, oldBase, &ok);
    if (!ok || newBase.trimmed().isEmpty() || newBase.trimmed() == oldBase)
        return;

    QString newPath = fi.absolutePath() + "/" + newBase.trimmed() + ".mp4";
    if (QFileInfo::exists(newPath)) {
        QMessageBox::warning(this, "Rename", "A file with that name already exists.");
        return;
    }

    if (!QFile::rename(fullPath, newPath))
        QMessageBox::warning(this, "Rename", "Could not rename file.");

    m_contextRow = -1;
    refresh();
}

void PlaybackManagerDialog::onDeleteFile() {
    if (m_contextRow < 0) return;
    QString fullPath = filePathAt(m_contextRow);
    if (fullPath.isEmpty()) return;

    QString name = QFileInfo(fullPath).fileName();
    auto answer = QMessageBox::question(this, "Delete",
        QString("Delete \"%1\"?").arg(name));
    if (answer != QMessageBox::Yes) return;

    if (!QFile::remove(fullPath))
        QMessageBox::warning(this, "Delete", "Could not delete file.");

    m_contextRow = -1;
    refresh();
}
