#include "filedialog.h"
#include "ui_filedialog.h"
#include <QDir>
#include <QFileIconProvider>

// Son kalınan klasörü hafızada tutmak için static değişken
static QString s_lastPath = "";

FileDialog::FileDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileDialog),
    m_model(new QFileSystemModel(this)),
    m_driveTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 1. Hatırlanan Klasörü Belirle
    if (s_lastPath.isEmpty())
        s_lastPath = QDir::homePath();

    m_model->setRootPath(s_lastPath);
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDot);
    
    QStringList filters;
    filters << "*.mp4" << "*.mkv" << "*.avi" << "*.mov" << "*.mpg" << "*.mpeg"
            << "*.wmv" << "*.flv" << "*.webm" << "*.mxf" << "*.ts" << "*.mts"
            << "*.m2ts" << "*.wav" << "*.mp3" << "*.aac";
    m_model->setNameFilters(filters);
    m_model->setNameFilterDisables(false);

    ui->listViewExp->setModel(m_model);
    ui->listViewExp->setRootIndex(m_model->index(s_lastPath));
    ui->listViewExp->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 2. İlk Sürücü Taraması ve Timer
    refreshDrives();
    connect(m_driveTimer, &QTimer::timeout, this, &FileDialog::refreshDrives);
    m_driveTimer->start(3000);

    // 3. Bağlantılar
    connect(ui->sidebarList, &QListWidget::itemSelectionChanged, this, &FileDialog::onSidebarSelectionChanged);
    connect(ui->listViewExp, &QListView::doubleClicked, this, &FileDialog::onExplorerActivated);
    connect(ui->addButton, &QPushButton::clicked, this, &FileDialog::onAddClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

FileDialog::~FileDialog() {
    delete ui;
}

void FileDialog::refreshDrives() {
    QString currentPath;
    if (ui->sidebarList->currentItem())
        currentPath = ui->sidebarList->currentItem()->data(Qt::UserRole).toString();

    QStringList newPaths;
    QList<QPair<QString, QString>> driveList;
    QFileIconProvider iconProvider;

    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && !storage.isReadOnly()) {
            QString path = storage.rootPath();
            bool isAllowed = (path == "/") || (path == "/home") ||
                             path.startsWith("/media") || path.startsWith("/mnt") ||
                             path.startsWith("/run/media");
            if (isAllowed) {
                QString name = storage.displayName();
                if (path == "/") name = "System (Root)";
                else if (path == "/home") name = "Users (Home)";
                else if (name.isEmpty()) name = path;
                
                driveList.append({name, path});
                newPaths << path;
            }
        }
    }

    // Artık m_lastDrives (member variable) kullanıyoruz, her pencere açılışında dolar
    if (newPaths == m_lastDrives) return;
    m_lastDrives = newPaths;

    ui->sidebarList->blockSignals(true);
    ui->sidebarList->clear();
    QListWidgetItem* toSelect = nullptr;

    for (const auto& drive : driveList) {
        QListWidgetItem* item = new QListWidgetItem(iconProvider.icon(QAbstractFileIconProvider::Drive), drive.first, ui->sidebarList);
        item->setData(Qt::UserRole, drive.second);
        if (drive.second == currentPath) toSelect = item;
        // Eğer s_lastPath bu sürücünün altındaysa başlangıçta bunu seçebiliriz (Opsiyonel)
    }
    
    if (toSelect) ui->sidebarList->setCurrentItem(toSelect);
    else if (ui->sidebarList->count() > 0) ui->sidebarList->setCurrentRow(0);

    ui->sidebarList->blockSignals(false);
}

void FileDialog::onSidebarSelectionChanged() {
    if (!ui->sidebarList->currentItem()) return;
    QString path = ui->sidebarList->currentItem()->data(Qt::UserRole).toString();
    s_lastPath = path; // Hafızaya al
    ui->listViewExp->setRootIndex(m_model->setRootPath(path));
}

void FileDialog::onExplorerActivated(const QModelIndex &index) {
    if (m_model->isDir(index)) {
        QString path = m_model->filePath(index);
        s_lastPath = path; // Hafızaya al
        ui->listViewExp->setRootIndex(m_model->setRootPath(path));
    } else {
        onAddClicked();
    }
}

void FileDialog::onAddClicked() {
    m_selectedPaths.clear();
    QModelIndexList selected = ui->listViewExp->selectionModel()->selectedIndexes();
    for (const QModelIndex &index : selected) {
        if (!m_model->isDir(index)) {
            m_selectedPaths << m_model->filePath(index);
        }
    }
    if (!m_selectedPaths.isEmpty()) {
        emit filesSelected(m_selectedPaths);
        accept();
    }
}

QStringList FileDialog::selectedFiles() const {
    return m_selectedPaths;
}
