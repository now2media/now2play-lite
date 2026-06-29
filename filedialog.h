#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QDialog>
#include <QFileSystemModel>
#include <QStorageInfo>
#include <QTimer>

namespace Ui {
class FileDialog;
}

class FileDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileDialog(QWidget *parent = nullptr);
    ~FileDialog();

    QStringList selectedFiles() const;

signals:
    void filesSelected(const QStringList &paths);

private slots:
    void onSidebarSelectionChanged();
    void onExplorerActivated(const QModelIndex &index);
    void refreshDrives();
    void onAddClicked();

private:
    Ui::FileDialog *ui;
    QFileSystemModel *m_model;
    QTimer *m_driveTimer;
    QStringList m_selectedPaths;
    QStringList m_lastDrives; // Sadece bu pencereye özel takip
};

#endif // FILEDIALOG_H
