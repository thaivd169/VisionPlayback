#pragma once
#include <QDialog>
#include <QLabel>
#include <QTableWidget>

class PlaybackManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PlaybackManagerDialog(const QString& downloadsPath,
                                   QWidget* parent = nullptr);

    QString selectedPath() const { return m_selectedPath; }

private slots:
    void onCellClicked(int row, int col);
    void onCustomContextMenu(const QPoint& pos);
    void onRenameFile();
    void onDeleteFile();

private:
    QString       m_downloadsPath;
    QTableWidget* m_table;
    QLabel*       m_totalLabel;

    QString m_selectedPath;
    int     m_contextRow = -1;

    void    refresh();
    QString filePathAt(int row) const;
};
