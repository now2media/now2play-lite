#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "LCharacter.h"
#include "LMixer.h"
#include "filedialog.h"
#include "rowcolordelegate.h"
#include "trimdialog.h"
#include "ui_settingsdialog.h"
#include <QAction>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QTextStream>
#include <algorithm>

#include <QAudioFormat>
#include <QAudioSink>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QStorageInfo>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

// Yardımcı: Milisaniye → HH:MM:SS:FF (Timecode formatı)
static QString msToTimecode(double ms, double fps) {
  if (ms < 0)
    ms = 0;
  if (fps <= 0)
    fps = 25.0;

  int total_f = qRound((ms * fps) / 1000.0);
  int ff = total_f % static_cast<int>(fps);
  int total_s = total_f / static_cast<int>(fps);
  int ss = total_s % 60;
  int mm = (total_s / 60) % 60;
  int hh = total_s / 3600;

  return QString::asprintf("%02d:%02d:%02d.%02d", hh, mm, ss, ff);
}

static double timecodeToMs(QString tc, double fps) {
  if (fps <= 0)
    fps = 25.0;
  tc.replace(".", ":");
  QStringList parts = tc.split(":");
  if (parts.size() != 4)
    return 0.0;

  int h = parts[0].toInt();
  int m = parts[1].toInt();
  int s = parts[2].toInt();
  int f = parts[3].toInt();

  double ms = (h * 3600.0 + m * 60.0 + s) * 1000.0;
  ms += (f / fps) * 1000.0;
  return ms;
}

// ─── Kurucu ─────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_playlistModel(new QStandardItemModel(this)), m_lFile(new LFile()),
      m_lReader(new LReader()), m_lPreviewLeft(new LPreview()),
      m_lPreviewMixer(new LPreview()), m_lMixer(new LMixer()),
      m_lOutput(new LOutput()), m_globalProps(new globalProperties()),
      m_uiTimer(new QTimer(this)), m_treeModel(new QFileSystemModel(this)),
      m_listModel(new QFileSystemModel(this)), m_testTimer(new QTimer(this)) {
  ui->setupUi(this);

  // --- FILE MENU ---
  QMenu *fileMenu = ui->menubar->addMenu("&File");

  QAction *openAction = new QAction(
      QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen), "Open List", this);
  fileMenu->addAction(openAction);
  connect(openAction, &QAction::triggered, this,
          &MainWindow::onOpenPlaylistClicked);

  QAction *addListAction = new QAction(
      QIcon::fromTheme(QIcon::ThemeIcon::FormatJustifyFill), "Add List", this);
  fileMenu->addAction(addListAction);
  connect(addListAction, &QAction::triggered, this,
          &MainWindow::onAddPlaylistClicked);

  QAction *addFileAction = new QAction(
      QIcon::fromTheme(QIcon::ThemeIcon::DocumentNew), "Add File", this);
  fileMenu->addAction(addFileAction);
  connect(addFileAction, &QAction::triggered, this, &MainWindow::onAddFile);

  fileMenu->addSeparator();

  QAction *saveAction = new QAction(
      QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave), "Save List", this);
  fileMenu->addAction(saveAction);
  connect(saveAction, &QAction::triggered, this,
          &MainWindow::onSavePlaylistClicked);

  QAction *emptyAction = new QAction(
      QIcon::fromTheme(QIcon::ThemeIcon::EditDelete), "Empty List", this);
  fileMenu->addAction(emptyAction);
  connect(emptyAction, &QAction::triggered, this,
          &MainWindow::onEmptyPlaylistClicked);

  fileMenu->addSeparator();

  QAction *quitAction = new QAction(
      QIcon::fromTheme(QIcon::ThemeIcon::SystemShutdown), "Quit", this);
  fileMenu->addAction(quitAction);
  connect(quitAction, &QAction::triggered, this, &MainWindow::close);

  // --- HELP MENU ---
  QMenu *helpMenu = ui->menubar->addMenu("&Help");
  QAction *aboutAction =
      new QAction(QIcon(":/now2playlogo.png"), "&About", this);
  helpMenu->addAction(aboutAction);
  connect(aboutAction, &QAction::triggered, this, [this]() {
#ifdef APP_VERSION
    QString version = QString(APP_VERSION);
#else
      QString version = "Unknown";
#endif
    QMessageBox::about(this, "About now2play",
                       QString("<h2>now2play Playout System</h2>"
                               "<p><b>Version:</b> %1</p>"
                               "<p>This software is a professional playout "
                               "automation system.</p>"
                               "<p>www.now2media.com<br>"
                               "support@now2media.com - info@now2media.com</p>"
                               "<p><i>Powered by <b>now2sdk</b> - Advanced Media Framework</i></p>"
                               "<p>Copyright &copy; 2026 now2media</p>")
                           .arg(version));
  });
  // ------------------------------

  // GlobalProps → seçili dosya ve durum için
  m_globalProps->currentState = globalProperties::Nothing;
  m_lFile->setProps("eof_hold", "true");

  videoFormatProps props;
  props.setVideoFormat = vF::HD1080_50i;
  // props.width = 320;
  // props.height = 180;
  // props.fps = 25.0;
  m_lMixer->setVideoFormat(props);
  m_lFile->setVideoFormat(props);

  videoFormatProps getProps;
  m_lMixer->getVideoFormat(getProps);

  int gWidth = getProps.width;
  int gHeight = getProps.height;

  audioFormatProps aProps;
  aProps.setAudioFormat = aF::_16B_48K_2CH;
  // m_lMixer->setAudioFormat(aProps);
  m_lFile->setAudioFormat(aProps);
  // Sol preview (playerPreview)
  m_lPreviewLeft->setProps("ui_framework", "qt");
  m_lPreviewLeft->previewEnable(ui->playerPreview, false, true);
  m_lPreviewLeft->previewObject(m_lFile);
  m_lPreviewLeft->setProps("audio_meter", "true");
  m_lPreviewLeft->setProps("timecode.preview", "true");
  m_lPreviewLeft->setProps("name", "Playlist");
  m_lPreviewLeft->setProps("status", "preview");



  /*colorFilter = new LFilter(LFilter::Video);
  delayfilter = new LFilter(LFilter::Audio);


  delayfilter->setProps("adelay","5500|5500");
  //m_lLive->addFilter(delayfilter);

  // Parlaklık (brightness): 0.1, Kontrast: 1.5, Satürasyon: 2.0 (Çok canlı
  renkler) colorFilter->setProps("eq",
  "brightness=1.0:contrast=3.0:saturation=2.5");

  //m_lLive->addFilter(delayfilter);*/

  // Mikser Önizleme
  m_lPreviewMixer->setProps("ui_framework", "qt");
  m_lPreviewMixer->previewEnable(ui->mixerPreview, false, true);
  m_lPreviewMixer->previewObject(m_lMixer);
  m_lPreviewMixer->setProps("audio_meter", "true");
  m_lPreviewMixer->setProps("name", "PGM");
  m_lPreviewMixer->setProps("status", "program");
  m_lMixer->addLayer("file", m_lFile, 1, 0, 0, 255, gWidth, gHeight, 0, 0, 0, 0,
                     0, "", true, 100);

  // Çıkış kaynağı olarak mikseri ayarlıyoruz
  m_lOutput->setSource(m_lMixer);
  // KJ (CG) Kurulumu
  m_lCG = new LCharacter();
  mainLogo = m_lCG->addItem("mainLogo", 0, 0, 0, 0, mainLogoProps);
  m_lMixer->addLayer("CG", m_lCG, 2, 0, 0, 255, gWidth, gHeight, 0, 0, 0, 0, 0,
                     "", false, 0);


  audioFormatProps audiooutprops;
  audiooutprops.setAudioFormat = aF::_16B_11K_2CH;
  m_lOutput->setProps("stream_name", "Playout-Output");
  m_lOutput->setProps(
      "NDI.color_format",
      "UYVY"); // Sadece NDI için geçerli renk seçimi BGRA / UYVY

  // Playlist tablosu
  m_playlistModel->setColumnCount(6);
  m_playlistModel->setHorizontalHeaderLabels(
      {"#", "Filename", "In Time", "Out Time", "Duration", "Resolution"});
  ui->playlistTableView->horizontalHeader()->setDefaultAlignment(
      Qt::AlignLeft | Qt::AlignVCenter);
  ui->playlistTableView->setModel(m_playlistModel);

  QHeaderView *header = ui->playlistTableView->horizontalHeader();
  ui->playlistTableView->verticalHeader()->setDefaultSectionSize(22);
  ui->playlistTableView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->playlistTableView, &QWidget::customContextMenuRequested, this,
          &MainWindow::onPlaylistContextMenuRequested);
  header->setSectionResizeMode(0, QHeaderView::Fixed);
  header->resizeSection(0, 40);
  header->setSectionResizeMode(1, QHeaderView::Stretch);
  header->setSectionResizeMode(2, QHeaderView::Fixed);
  header->resizeSection(2, 150);
  header->setSectionResizeMode(3, QHeaderView::Fixed);
  header->resizeSection(3, 150);
  header->setSectionResizeMode(4, QHeaderView::Fixed);
  header->resizeSection(4, 150);
  header->setSectionResizeMode(5, QHeaderView::Fixed);
  header->resizeSection(5, 200);
  ui->playlistTableView->setItemDelegate(
      new RowColorDelegate(m_globalProps, this));

  // Buton ve ComboBox bağlantıları
  connect(ui->addFile, &QPushButton::clicked, this, &MainWindow::onAddFile);
  connect(ui->playBtn, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
  connect(ui->pauseBtn, &QPushButton::clicked, this,
          &MainWindow::onPauseClicked);
  connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);

  m_settings = new QSettings("now2play", "now2playApp", this);
  m_settingsDialog = new SettingsDialog(this);

  // LOutput Bağlantıları
  connect(m_settingsDialog->ui->outDeviceComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) { onOutDeviceChanged(index); });
  connect(m_settingsDialog->ui->outDeviceChannelComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) { onOutChannelChanged(index); });
  connect(m_settingsDialog->ui->outDeviceFormatComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) { onOutFormatChanged(index); });
  connect(m_settingsDialog->ui->outDeviceAudioFormatComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) { onOutAudioFormatChanged(index); });

  // Ayarların Anında (Canlı) Uygulanması
  connect(m_settingsDialog->ui->outputEnableCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) {
            m_outputEnabled = checked;
            m_lOutput->setEnabled(checked);
            m_settings->setValue("outputEnabled", checked);
          });

  connect(m_settingsDialog->ui->Autonextmedia, &QCheckBox::toggled, this,
          [this](bool checked) {
            m_autoNextMedia = checked;
            m_settings->setValue("Autonextmedia", checked);
          });

  connect(m_settingsDialog->ui->playlistloop, &QCheckBox::toggled, this,
          [this](bool checked) {
            m_playlistLoop = checked;
            m_settings->setValue("playlistloop", checked);
            updateButtonStates();
          });

  connect(m_settingsDialog->ui->nextmediapause, &QCheckBox::toggled, this,
          [this](bool checked) {
            m_nextMediaPause = checked;
            m_settings->setValue("nextmediapause", checked);
          });

  connect(m_settingsDialog->ui->deinterlaceChk, &QCheckBox::toggled, this,
          [this](bool checked) {
            m_deinterlaceEnabled = checked;
            m_settings->setValue("deinterlace", checked);
            m_lFile->setProps("deinterlace", checked ? "true" : "false");
          });

  connect(m_settingsDialog->ui->gpuDecodingChk, &QCheckBox::toggled, this,
          [this](bool checked) {
            m_gpuDecoding = checked;
            m_settings->setValue("gpuDecoding", checked);
            m_lFile->setProps("gpu", checked ? "true" : "false");
          });

  connect(m_settingsDialog->ui->scaleTypeCombo, &QComboBox::currentTextChanged,
          this, [this](const QString &text) {
            m_scaleType = text;
            m_settings->setValue("scaleType", text);
            m_lFile->setProps("scale_type", text.toStdString());
          });

  connect(ui->playlistTableView, &QTableView::doubleClicked, this,
          &MainWindow::onRowDoubleClicked);

  connect(ui->settingsBtn, &QPushButton::clicked, this,
          &MainWindow::onSettingsClicked);
  connect(ui->nextBtn, &QPushButton::clicked, this, &MainWindow::onNextClicked);
  connect(ui->backBtn, &QPushButton::clicked, this, &MainWindow::onBackClicked);
  connect(ui->btnMutePreview, &QPushButton::clicked, this,
          &MainWindow::onMutePreviewClicked);

  connect(ui->emptyPlaylist, &QPushButton::clicked, this,
          &MainWindow::onEmptyPlaylistClicked);
  connect(ui->savePlaylist, &QPushButton::clicked, this,
          &MainWindow::onSavePlaylistClicked);
  connect(ui->openPlaylist, &QPushButton::clicked, this,
          &MainWindow::onOpenPlaylistClicked);
  connect(ui->addPlaylist, &QPushButton::clicked, this,
          &MainWindow::onAddPlaylistClicked);

  loadSettings();

  // İlk açılışta cihazları tara (Örnekteki gibi tek seferlik tarama)
  refreshOutputSources();

  // Arayüz ve cihazlar hazır olduktan sonra çıkışı etkinleştir
  m_lOutput->setEnabled(m_outputEnabled);

  // UI timer
  connect(m_uiTimer, &QTimer::timeout, this, &MainWindow::updateUI);
  m_uiTimer->start(50);

  updateButtonStates();
  refreshDrives();

  QTimer *driveTimer = new QTimer(this);
  connect(driveTimer, &QTimer::timeout, this, &MainWindow::refreshDrives);
  driveTimer->start(3000);

  QString initialPath = QDir::homePath();
  m_listModel->setRootPath(initialPath);
  m_listModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDot);
  QStringList filters;
  filters << "*.mp4" << "*.mkv" << "*.avi" << "*.mov" << "*.mpg" << "*.mpeg"
          << "*.wmv" << "*.flv" << "*.webm" << "*.mxf" << "*.ts" << "*.mts"
          << "*.m2ts" << "*.wav" << "*.mp3" << "*.aac";
  m_listModel->setNameFilters(filters);
  m_listModel->setNameFilterDisables(false);

  ui->listViewExp->setModel(m_listModel);
  ui->listViewExp->setRootIndex(m_listModel->index(initialPath));
  ui->listViewExp->setDragEnabled(true);

  connect(ui->driveComboBox, QOverload<int>::of(&QComboBox::activated), this,
          &MainWindow::onDriveChanged);
  connect(ui->listViewExp, &QListView::doubleClicked, this,
          &MainWindow::onExplorerActivated);

  // Sürükle-Bırak
  ui->playlistTableView->setAcceptDrops(true);
  ui->playlistTableView->setDragEnabled(true);
  ui->playlistTableView->setDragDropMode(QAbstractItemView::InternalMove);
  ui->playlistTableView->setDropIndicatorShown(true);
  ui->playlistTableView->setDefaultDropAction(Qt::MoveAction);
  ui->playlistTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->playlistTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  ui->playlistTableView->setDragDropOverwriteMode(false);
  ui->playlistTableView->installEventFilter(this);
  ui->playlistTableView->viewport()->installEventFilter(this);

  updateUI();

  setWindowTitle("now2play-lite - " + QString(APP_VERSION) + " | [now2media]");
}

// ─── Örnek (Example) Projesindeki Birebir Aynı Çıkış Altyapısı
// ────────────────
void MainWindow::refreshOutputSources() {
  m_settingsDialog->ui->outDeviceComboBox->blockSignals(true);
  m_settingsDialog->ui->outDeviceComboBox->clear();

  // Audio Formats Setup
  if (m_settingsDialog->ui->outDeviceAudioFormatComboBox->count() == 0) {
    m_settingsDialog->ui->outDeviceAudioFormatComboBox->blockSignals(true);

#define X(name)                                                                \
  if (QString(#name).startsWith("_")) {                                        \
    m_settingsDialog->ui->outDeviceAudioFormatComboBox->addItem(               \
        QString(#name).mid(1), (int)aF::name);                                 \
  }
    AF_VALUES
#undef X

    int defaultIdx =
        m_settingsDialog->ui->outDeviceAudioFormatComboBox->findData(
            (int)aF::_16B_48K_2CH);
    if (defaultIdx < 0)
      defaultIdx = 0;

    int savedAudioFormat =
        m_settings->value("outAudioFormat", defaultIdx).toInt();
    if (savedAudioFormat >=
        m_settingsDialog->ui->outDeviceAudioFormatComboBox->count())
      savedAudioFormat = defaultIdx;

    m_settingsDialog->ui->outDeviceAudioFormatComboBox->setCurrentIndex(
        savedAudioFormat);
    m_settingsDialog->ui->outDeviceAudioFormatComboBox->blockSignals(false);
    onOutAudioFormatChanged(savedAudioFormat);
  }

  int count = 0;
  m_lOutput->DeviceGetCount(count);

  for (int i = 0; i < count; i++) {
    std::string name, desc;
    m_lOutput->DeviceGetByIndex(i, name, desc);
    m_settingsDialog->ui->outDeviceComboBox->addItem(
        QString::fromStdString(name), i);
  }

  m_settingsDialog->ui->outDeviceComboBox->blockSignals(false);

  if (count > 0) {
    int savedDevice = m_settings->value("outDevice", 0).toInt();
    if (savedDevice >= count)
      savedDevice = 0;
    m_settingsDialog->ui->outDeviceComboBox->setCurrentIndex(savedDevice);
    onOutDeviceChanged(savedDevice);
  }
}

void MainWindow::onOutDeviceChanged(int index) {
  // m_lOutput->setEnabled(false);
  if (index < 0)
    return;
  int deviceIndex =
      m_settingsDialog->ui->outDeviceComboBox->itemData(index).toInt();
  m_lOutput->setDevice(deviceIndex);

  m_settingsDialog->ui->outDeviceChannelComboBox->blockSignals(true);
  m_settingsDialog->ui->outDeviceChannelComboBox->clear();

  int count = 0;
  m_lOutput->DeviceChannelGetCount(deviceIndex, count);

  for (int i = 0; i < count; i++) {
    std::string name, desc;
    m_lOutput->DeviceChannelGetByIndex(deviceIndex, i, name, desc);
    m_settingsDialog->ui->outDeviceChannelComboBox->addItem(
        QString::fromStdString(name), i);
  }

  m_settingsDialog->ui->outDeviceChannelComboBox->blockSignals(false);

  if (count > 0) {
    int savedChannel = m_settings->value("outChannel", 0).toInt();
    if (savedChannel >= count)
      savedChannel = 0;
    m_settingsDialog->ui->outDeviceChannelComboBox->setCurrentIndex(
        savedChannel);
    onOutChannelChanged(savedChannel);
  }
  // m_lOutput->setEnabled(m_outputEnabled);
}

void MainWindow::onOutChannelChanged(int index) {
  // m_lOutput->setEnabled(false);
  if (index < 0)
    return;
  int channelIndex =
      m_settingsDialog->ui->outDeviceChannelComboBox->itemData(index).toInt();
  m_lOutput->DeviceChannelSet(channelIndex);

  int deviceIndex =
      m_settingsDialog->ui->outDeviceComboBox->currentData().toInt();

  m_settingsDialog->ui->outDeviceFormatComboBox->blockSignals(true);
  m_settingsDialog->ui->outDeviceFormatComboBox->clear();

  int count = 0;
  int defaultFormatIdx = 0;
  m_lOutput->DeviceFormatVideoGetCount(deviceIndex, channelIndex, count);

  for (int i = 0; i < count; i++) {
    std::string name;
    videoFormatProps props;
    m_lOutput->DeviceFormatVideoGetByIndex(deviceIndex, channelIndex, i, name,
                                           props);
    m_settingsDialog->ui->outDeviceFormatComboBox->addItem(
        QString::fromStdString(name), i);

    if (name.find("1080") != std::string::npos &&
        (name.find("50i") != std::string::npos ||
         name.find("i50") != std::string::npos)) {
      defaultFormatIdx = i;
    }
  }

  m_settingsDialog->ui->outDeviceFormatComboBox->blockSignals(false);

  if (count > 0) {
    int savedFormat = m_settings->value("outFormat", -1).toInt();
    if (savedFormat >= 0 && savedFormat < count) {
      defaultFormatIdx = savedFormat;
    }
    m_settingsDialog->ui->outDeviceFormatComboBox->setCurrentIndex(
        defaultFormatIdx);
    onOutFormatChanged(defaultFormatIdx);
  }
  // m_lOutput->setEnabled(m_outputEnabled);
}

void MainWindow::onOutFormatChanged(int index) {
  // m_lOutput->setEnabled(false);
  if (index < 0)
    return;
  int formatIndex =
      m_settingsDialog->ui->outDeviceFormatComboBox->itemData(index).toInt();

  int deviceIndex =
      m_settingsDialog->ui->outDeviceComboBox->currentData().toInt();
  int channelIndex =
      m_settingsDialog->ui->outDeviceChannelComboBox->currentData().toInt();

  std::string name;
  videoFormatProps props;
  m_lOutput->DeviceFormatVideoGetByIndex(deviceIndex, channelIndex, formatIndex,
                                         name, props);

  m_lOutput->DeviceFormatVideoSet(formatIndex);

  m_lMixer->setVideoFormat(props);
  m_lFile->setVideoFormat(props);
}

void MainWindow::onOutAudioFormatChanged(int index) {
  if (index < 0)
    return;

  int enumVal =
      m_settingsDialog->ui->outDeviceAudioFormatComboBox->itemData(index)
          .toInt();

  audioFormatProps props;
  props.setAudioFormat = (aF_Type)enumVal;

  m_lOutput->DeviceFormatAudioSet(props);
  // m_lMixer->setAudioFormat(props);
  m_lFile->setAudioFormat(props);
}

// ─── Yıkıcı ─────────────────────────────────────────────────────────────────
MainWindow::~MainWindow() {
  if (m_lFile)
    m_lFile->stop();

  delete m_lPreviewLeft;
  delete m_lPreviewMixer;
  delete m_lFile;
  delete m_lMixer;
  delete m_lReader;
  delete m_globalProps;
  delete ui;
}

// ─── Dosya Ekleme ────────────────────────────────────────────────────────────
void MainWindow::onAddFile() {
  FileDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    for (const QString &fp : dlg.selectedFiles())
      addFileToPlaylist(fp);
  }
}

void MainWindow::addFileToPlaylist(const QString &filePath, int row) {
  QThreadPool::globalInstance()->start([this, filePath, row]() {
    if (filePath.startsWith("CMD:")) {
      QString cmdType = filePath.mid(4);
      QMetaObject::invokeMethod(
          this,
          [this, row, filePath, cmdType]() {
            int finalRow = (row < 0 || row > m_playlistModel->rowCount())
                               ? m_playlistModel->rowCount()
                               : row;
            auto *posItem = new QStandardItem(QString::number(finalRow + 1));
            posItem->setData(filePath, Qt::UserRole);
            QList<QStandardItem *> rowItems = {
                posItem,
                new QStandardItem("[command] - " + cmdType),
                new QStandardItem("-"),
                new QStandardItem("-"),
                new QStandardItem("-"),
                new QStandardItem("-")};
            m_playlistModel->insertRow(finalRow, rowItems);
            for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
              if (auto *it = m_playlistModel->item(i, 0))
                it->setText(QString::number(i + 1));
            }
            updateButtonStates();
            ui->playlistTableView->viewport()->update();
            if (m_playlistModel->rowCount() == 1)
              selectFile(0);
          },
          Qt::QueuedConnection);
      return;
    }

    QFileInfo fi(filePath);
    if (!fi.exists())
      return;

    // LReader nesnesini thread-safe olması için yerel oluşturuyoruz
    LReader probe;
    if (probe.open(filePath.toStdString())) {
      double durMs = probe.getDurationMs();
      double fps = probe.getFps();
      int width = probe.getWidth();
      int height = probe.getHeight();
      double inMs = probe.getInPoint();
      double outMs = probe.getOutPoint();
      QString scanType = QString::fromStdString(probe.getScanType());
      QString scanChar = (scanType == "Progressive") ? "p" : "i";
      QString resFinal =
          QString("%1%2%3").arg(height).arg(scanChar).arg(qRound(fps));

      QMetaObject::invokeMethod(
          this,
          [this, row, filePath, fi, inMs, outMs, durMs, fps, resFinal]() {
            int finalRow = (row < 0 || row > m_playlistModel->rowCount())
                               ? m_playlistModel->rowCount()
                               : row;

            // UI verilerini güncelle
            auto *posItem = new QStandardItem(QString::number(finalRow + 1));
            posItem->setData(filePath, Qt::UserRole);

            QList<QStandardItem *> rowItems = {
                posItem,
                new QStandardItem(fi.fileName()),
                new QStandardItem(msToTimecode(inMs, fps)),
                new QStandardItem(msToTimecode(outMs, fps)),
                new QStandardItem(msToTimecode(durMs, fps)),
                new QStandardItem(resFinal)};
            m_playlistModel->insertRow(finalRow, rowItems);

            for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
              if (auto *it = m_playlistModel->item(i, 0))
                it->setText(QString::number(i + 1));
            }

            updateButtonStates();
            ui->playlistTableView->viewport()->update();

            if (m_playlistModel->rowCount() == 1) {
              selectFile(0);
              // m_lFile->ready(); // SIGSEGV riskine karşı şimdilik devre dışı
              // veya kontrollü
            }
          },
          Qt::QueuedConnection);
    }
  });
}

// ─── Dosya Seçimi ────────────────────────────────────────────────────────────
void MainWindow::selectFile(int index) {
  if (index < 0 || index >= m_playlistModel->rowCount())
    return;

  m_currentIndex = index;
  m_globalProps->selectfile = index;
  m_elapsedFrames = 0;

  QString pathOrCmd =
      m_playlistModel->item(index, 0)->data(Qt::UserRole).toString();

  // YENİ: Komut (Command) Satırı Kontrolü
  if (pathOrCmd.startsWith("CMD:")) {
    updateButtonStates();
    ui->playlistTableView->selectRow(index);
    ui->playlistTableView->viewport()->update();

    QString cmd = pathOrCmd.mid(4);
    if (cmd == "STOP") {
      onStopClicked();
    } else if (cmd == "PAUSE") {
      if (m_globalProps->currentState == globalProperties::Playing) {
        onPauseClicked();
      }
    } else if (cmd == "LOOP") {
      int prevVideoIdx = -1;
      for (int i = index - 1; i >= 0; --i) {
        QString p = m_playlistModel->item(i, 0)->data(Qt::UserRole).toString();
        if (!p.startsWith("CMD:")) {
          prevVideoIdx = i;
          break;
        }
      }
      if (prevVideoIdx >= 0) {
        selectFile(prevVideoIdx);
      } else {
        onStopClicked();
      }
    } else if (cmd == "PLAYLIST_LOOP") {
      if (m_playlistModel->rowCount() > 0) {
        selectFile(0);
      }
    }
    return; // LFile ile işlem yapma, komut tamamlandı
  }

  // ESKİ: Normal Dosya Yükleme Mantığı
  QString path = pathOrCmd;
  double inMs = 0;
  QStringList parts = pathOrCmd.split("|");
  if (parts.size() >= 3) {
    path = parts[0];
    inMs = parts[1].toDouble();
  }

  m_lFile->stop();
  m_lFile->fileNameSet(path.toStdString());
  m_lFile->PosSet(inMs);

  ui->fileLabel->setText(m_playlistModel->item(index, 1)->text());

  if (m_globalProps->currentState == globalProperties::Playing) {
    m_lFile->play();
    m_lPreviewLeft->previewObject(m_lFile);
  } else {
    m_globalProps->currentState = globalProperties::Stopped;
  }

  updateButtonStates();
  ui->playlistTableView->viewport()->update();
}

void MainWindow::onRowDoubleClicked(const QModelIndex &index) {
  if (index.isValid()) {
    selectFile(index.row());
    if (m_globalProps->currentState != globalProperties::Playing) {
      m_lFile->play();
      m_lFile->pause();
      m_lPreviewLeft->previewObject(m_lFile);
      m_globalProps->currentState = globalProperties::Paused;
      updateButtonStates();
    }
  }
}

// ─── Transport Kontrolleri ───────────────────────────────────────────────────
void MainWindow::onPlayClicked() {
  if (m_playlistModel->rowCount() == 0)
    return;

  // İlk defa oynatılıyorsa dosyayı aç
  if (m_currentIndex < 0)
    selectFile(0);

  m_globalProps->currentState = globalProperties::Playing;

  m_lFile->play();

  updateButtonStates();
}

void MainWindow::onPauseClicked() {
  // LFile'ın pause desteği yok — preview heartbeat'i durduruyoruz
  m_lFile->pause();
  m_globalProps->currentState = globalProperties::Paused;
  updateButtonStates();
}

void MainWindow::onStopClicked() {
  // m_lPreviewLeft->stop();
  m_lFile->stop();
  m_elapsedFrames = 0;
  m_globalProps->currentState = globalProperties::Stopped;
  updateButtonStates();
  // updateUI();
}

// ─── Buton Durumları ─────────────────────────────────────────────────────────
void MainWindow::updateButtonStates() {
  bool hasFiles = m_playlistModel->rowCount() > 0;
  ui->playBtn->setEnabled(hasFiles && m_globalProps->currentState !=
                                          globalProperties::Playing);
  ui->pauseBtn->setEnabled(m_globalProps->currentState ==
                           globalProperties::Playing);
  ui->stopBtn->setEnabled(hasFiles);

  ui->emptyPlaylist->setEnabled(hasFiles);
  ui->savePlaylist->setEnabled(hasFiles);

  if (hasFiles && m_currentIndex >= 0) {
    ui->backBtn->setEnabled(m_currentIndex > 0 || m_playlistLoop);
    ui->nextBtn->setEnabled(m_currentIndex < m_playlistModel->rowCount() - 1 ||
                            m_playlistLoop);
  } else {
    ui->backBtn->setEnabled(false);
    ui->nextBtn->setEnabled(false);
  }
}

void MainWindow::onSettingsClicked() {
  // Diyalog açılırken gereksiz "Anında Uygulama" sinyallerini blokla
  m_settingsDialog->blockSignals(true);
  const auto widgets = m_settingsDialog->findChildren<QWidget *>();
  for (QWidget *w : widgets) {
    w->blockSignals(true);
  }

  // Kaynakları burada tazelemeye gerek yok, kurucuda 1 kez taranıyor (Örnekteki
  // gibi)

  // UI değerlerini açılmadan önce güncelle (Comboboxlar zaten senkronize)
  m_settingsDialog->ui->Autonextmedia->setChecked(m_autoNextMedia);
  m_settingsDialog->ui->playlistloop->setChecked(m_playlistLoop);
  m_settingsDialog->ui->nextmediapause->setChecked(m_nextMediaPause);
  m_settingsDialog->ui->outputEnableCheckBox->setChecked(m_outputEnabled);
  m_settingsDialog->ui->deinterlaceChk->setChecked(m_deinterlaceEnabled);
  m_settingsDialog->ui->scaleTypeCombo->setCurrentText(m_scaleType);
  m_settingsDialog->ui->gpuDecodingChk->setChecked(m_gpuDecoding);

  // İşlem bitince sinyalleri geri aç
  for (QWidget *w : widgets) {
    w->blockSignals(false);
  }
  m_settingsDialog->blockSignals(false);

  if (m_settingsDialog->exec() == QDialog::Accepted) {
    saveSettings();
  }
}

void MainWindow::onNextClicked() {
  if (m_playlistModel->rowCount() == 0)
    return;
  int nextIndex = m_currentIndex + 1;
  if (nextIndex >= m_playlistModel->rowCount()) {
    if (m_playlistLoop)
      nextIndex = 0;
    else
      return;
  }

  bool shouldPause = m_nextMediaPause;
  if (shouldPause) {
    m_globalProps->currentState = globalProperties::Playing;
  }

  selectFile(nextIndex);

  if (shouldPause) {
    QTimer::singleShot(50, this, &MainWindow::onPauseClicked);
  }
}

void MainWindow::onBackClicked() {
  if (m_playlistModel->rowCount() == 0)
    return;
  int prevIndex = m_currentIndex - 1;
  if (prevIndex < 0) {
    if (m_playlistLoop)
      prevIndex = m_playlistModel->rowCount() - 1;
    else
      return;
  }
  selectFile(prevIndex);
}

void MainWindow::onEmptyPlaylistClicked() {
  if (m_playlistModel->rowCount() == 0)
    return;

  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(this, "Empty Playlist",
                                "Are you sure you want to empty the playlist?",
                                QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    if (m_globalProps->currentState == globalProperties::Playing ||
        m_globalProps->currentState == globalProperties::Paused) {
      onStopClicked();
    }
    m_playlistModel->removeRows(0, m_playlistModel->rowCount());
    m_currentIndex = -1;
    m_lPreviewLeft->clear();
    updateButtonStates();
    updateUI();
  }
}

void MainWindow::onSavePlaylistClicked() {
  if (m_playlistModel->rowCount() == 0)
    return;

  QString fileName = QFileDialog::getSaveFileName(
      this, "Save Playlist", "", "now2play Playlist (*.nplist)");
  if (fileName.isEmpty())
    return;

  QFile file(fileName);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
      QString path = m_playlistModel->item(i, 0)->data(Qt::UserRole).toString();
      out << path << "\n";
    }
    file.close();
  }
}

void MainWindow::onOpenPlaylistClicked() {
  if (m_playlistModel->rowCount() > 0) {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Open Playlist",
                                  "Opening a new playlist will clear the "
                                  "current list.\nDo you want to continue?",
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
      return;
  }

  QString fileName = QFileDialog::getOpenFileName(
      this, "Open Playlist", "", "now2play Playlist (*.nplist)");
  if (fileName.isEmpty())
    return;

  loadPlaylistFile(fileName, true);
}

void MainWindow::onAddPlaylistClicked() {
  QString fileName = QFileDialog::getOpenFileName(
      this, "Add Playlist", "", "now2play Playlist (*.nplist)");
  if (fileName.isEmpty())
    return;

  loadPlaylistFile(fileName, false);
}

void MainWindow::loadPlaylistFile(const QString &fileName, bool clearFirst,
                                  int targetRow) {
  QThreadPool::globalInstance()->start([this, fileName, clearFirst,
                                        targetRow]() {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return;
    QTextStream in(&file);
    QStringList items;
    while (!in.atEnd()) {
      QString line = in.readLine().trimmed();
      if (!line.isEmpty())
        items.append(line);
    }
    file.close();

    QList<QStringList> parsedRows;
    for (const QString &path : items) {
      if (path.startsWith("CMD:")) {
        QStringList rowData;
        rowData << path << ("[COMMAND] - " + path.mid(4)) << "-" << "-" << "-"
                << "-";
        parsedRows.append(rowData);
      } else {
        QString realPath = path;
        double inMs = -1, outMs = -1;
        QStringList parts = path.split("|");
        if (parts.size() >= 3) {
          realPath = parts[0];
          inMs = parts[1].toDouble();
          outMs = parts[2].toDouble();
        }
        QFileInfo fi(realPath);
        if (fi.exists()) {
          LReader probe;
          if (probe.open(realPath.toStdString())) {
            double durMs = probe.getDurationMs();
            double fps = probe.getFps();
            if (inMs < 0)
              inMs = probe.getInPoint();
            if (outMs < 0)
              outMs = probe.getOutPoint(); // which is durMs in LReader

            QStringList rowData;
            rowData << path << fi.fileName() << msToTimecode(inMs, fps)
                    << msToTimecode(outMs, fps)
                    << msToTimecode(outMs - inMs, fps)
                    << QString("%1%2%3")
                           .arg(probe.getHeight())
                           .arg((probe.getScanType() == "Progressive") ? "p"
                                                                       : "i")
                           .arg(qRound(fps));
            parsedRows.append(rowData);
          }
        }
      }
    }

    QMetaObject::invokeMethod(
        this,
        [this, parsedRows, clearFirst, targetRow]() {
          if (clearFirst) {
            if (m_globalProps->currentState == globalProperties::Playing) {
              onStopClicked();
            }
            m_playlistModel->removeRows(0, m_playlistModel->rowCount());
            m_currentIndex = -1;
          }

          bool wasEmpty = (m_playlistModel->rowCount() == 0);
          int startRow =
              (targetRow < 0 || targetRow > m_playlistModel->rowCount())
                  ? m_playlistModel->rowCount()
                  : targetRow;

          for (int i = 0; i < parsedRows.size(); ++i) {
            const QStringList &r = parsedRows[i];
            auto *posItem =
                new QStandardItem(QString::number(startRow + i + 1));
            posItem->setData(r[0], Qt::UserRole);
            QList<QStandardItem *> rowItems = {posItem,
                                               new QStandardItem(r[1]),
                                               new QStandardItem(r[2]),
                                               new QStandardItem(r[3]),
                                               new QStandardItem(r[4]),
                                               new QStandardItem(r[5])};
            m_playlistModel->insertRow(startRow + i, rowItems);
          }

          for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
            if (auto *it = m_playlistModel->item(i, 0))
              it->setText(QString::number(i + 1));
          }

          updateButtonStates();
          ui->playlistTableView->viewport()->update();

          if (wasEmpty && m_playlistModel->rowCount() > 0) {
            selectFile(0);
          }
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::onMutePreviewClicked() {
  m_previewMuted = !m_previewMuted;

  if (m_previewMuted) {
    m_lPreviewLeft->mute(true);
    ui->btnMutePreview->setIcon(
        QIcon::fromTheme(QIcon::ThemeIcon::AudioVolumeMuted));
    ui->btnMutePreview->setStyleSheet(
        "QPushButton {\n"
        "  border: 2px solid rgb(170, 0, 0);\n"
        "  background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, "
        "y2:1, stop:0 rgb(40, 40, 40), stop:1 rgb(60, 60, 60));\n"
        "  border-radius: 5px;\n"
        "  color: rgb(0, 195, 0);\n"
        "  font-weight: bold;\n"
        "}");
  } else {
    m_lPreviewLeft->mute(false);
    ui->btnMutePreview->setIcon(
        QIcon::fromTheme(QIcon::ThemeIcon::AudioVolumeHigh));
    ui->btnMutePreview->setStyleSheet(
        "QPushButton {\n"
        "  border: 2px solid rgb(0, 0, 0);\n"
        "  background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, "
        "y2:1, stop:0 rgba(60, 60, 60, 255), stop:1 rgba(40, 40, 40, 255));\n"
        "  border-radius: 5px;\n"
        "  color: rgb(255, 255, 255);\n"
        "  font-weight: bold;\n"
        "}");
  }
}

void MainWindow::loadSettings() {
  m_autoNextMedia = m_settings->value("Autonextmedia", true).toBool();
  m_playlistLoop = m_settings->value("playlistloop", false).toBool();
  m_nextMediaPause = m_settings->value("nextmediapause", false).toBool();
  m_outputEnabled = m_settings->value("outputEnabled", false).toBool();
  m_deinterlaceEnabled = m_settings->value("deinterlace", true).toBool();
  m_scaleType = m_settings->value("scaleType", "aspect-ratio").toString();
  m_gpuDecoding = m_settings->value("gpuDecoding", false).toBool();

  m_lFile->setProps("deinterlace", m_deinterlaceEnabled ? "true" : "false");
  m_lFile->setProps("scale_type", m_scaleType.toStdString());
  m_lFile->setProps("gpu", m_gpuDecoding ? "true" : "false");
}

void MainWindow::saveSettings() {
  m_autoNextMedia = m_settingsDialog->ui->Autonextmedia->isChecked();
  m_playlistLoop = m_settingsDialog->ui->playlistloop->isChecked();
  m_nextMediaPause = m_settingsDialog->ui->nextmediapause->isChecked();
  m_outputEnabled = m_settingsDialog->ui->outputEnableCheckBox->isChecked();
  m_deinterlaceEnabled = m_settingsDialog->ui->deinterlaceChk->isChecked();
  m_scaleType = m_settingsDialog->ui->scaleTypeCombo->currentText();
  m_gpuDecoding = m_settingsDialog->ui->gpuDecodingChk->isChecked();

  m_settings->setValue("Autonextmedia", m_autoNextMedia);
  m_settings->setValue("playlistloop", m_playlistLoop);
  m_settings->setValue("nextmediapause", m_nextMediaPause);
  m_settings->setValue("outputEnabled", m_outputEnabled);
  m_settings->setValue("deinterlace", m_deinterlaceEnabled);
  m_settings->setValue("scaleType", m_scaleType);
  m_settings->setValue("gpuDecoding", m_gpuDecoding);

  m_lOutput->setEnabled(m_outputEnabled);
  m_lFile->setProps("deinterlace", m_deinterlaceEnabled ? "true" : "false");
  m_lFile->setProps("scale_type", m_scaleType.toStdString());
  m_lFile->setProps("gpu", m_gpuDecoding ? "true" : "false");

  m_settings->setValue("outDevice",
                       m_settingsDialog->ui->outDeviceComboBox->currentIndex());
  m_settings->setValue(
      "outChannel",
      m_settingsDialog->ui->outDeviceChannelComboBox->currentIndex());
  m_settings->setValue(
      "outFormat",
      m_settingsDialog->ui->outDeviceFormatComboBox->currentIndex());
  m_settings->setValue(
      "outAudioFormat",
      m_settingsDialog->ui->outDeviceAudioFormatComboBox->currentIndex());

  updateButtonStates();
}

// ─── UI Güncelleme (50ms'de bir) ─────────────────────────────────────────────
void MainWindow::updateUI() {
  bool hasFiles = m_playlistModel->rowCount() > 0;

  double targetFps = m_lFile->getFPS();
  if (targetFps <= 0)
    targetFps = 25.0;

  double originalFps = targetFps;
  if (hasFiles && m_currentIndex >= 0 &&
      m_playlistModel->item(m_currentIndex, 5)) {
    QString resStr = m_playlistModel->item(m_currentIndex, 5)->text();
    int lastIdx = -1;
    if (resStr.contains("p"))
      lastIdx = resStr.lastIndexOf("p");
    else if (resStr.contains("i"))
      lastIdx = resStr.lastIndexOf("i");

    if (lastIdx != -1) {
      originalFps = resStr.mid(lastIdx + 1).toDouble();
    }
  }
  if (originalFps <= 0)
    originalFps = targetFps;

  // SDK bug telafisi: LFile target formatta çalışırken PosGet yanlış hızda ms
  // verir. Gerçek ms = PosGet() * (originalFps / targetFps)
  double curMs = m_lFile->PosGet() * (originalFps / targetFps);
  double durMs = m_lFile->getDurationMs();
  double fps = originalFps; // msToTimecode için gerçek fps'yi kullan

  double outMs = durMs;
  if (hasFiles && m_currentIndex >= 0) {
    QString pathOrCmd =
        m_playlistModel->item(m_currentIndex, 0)->data(Qt::UserRole).toString();
    if (!pathOrCmd.startsWith("CMD:")) {
      QStringList parts = pathOrCmd.split("|");
      if (parts.size() >= 3) {
        outMs = parts[2].toDouble();
      }
    }
  }

  // --- Oynatma Listesi Zaman Hesaplaması ---
  double totalPlaylistMs = 0;
  double playlistCurMs = 0;

  if (hasFiles) {
    for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
      if (QStandardItem *item = m_playlistModel->item(i, 4)) {
        QString tc = item->text();
        int hh = 0, mm = 0, ss = 0, ff = 0;
        sscanf(tc.toStdString().c_str(), "%d:%d:%d.%d", &hh, &mm, &ss, &ff);
        double ms = (hh * 3600 + mm * 60 + ss) * 1000.0 + (ff * 1000.0 / 25.0);
        totalPlaylistMs += ms;

        if (i < m_currentIndex) {
          playlistCurMs += ms;
        } else if (i == m_currentIndex) {
          playlistCurMs += curMs;
        }
      }
    }
  }

  double playlistRemainingMs = totalPlaylistMs - playlistCurMs;
  if (playlistRemainingMs < 0)
    playlistRemainingMs = 0;

  if (hasFiles) {
    ui->PlaylistProgressBar->setMaximum(
        totalPlaylistMs > 0 ? static_cast<int>(totalPlaylistMs) : 1);
    ui->PlaylistProgressBar->setValue(static_cast<int>(playlistCurMs));
    ui->PlaylistProgressBar->setFormat(msToTimecode(playlistCurMs, fps));
    ui->pCurTimeLbl->setText(msToTimecode(playlistCurMs, fps));
    ui->pDurTimeLbl->setText(msToTimecode(totalPlaylistMs, fps));
    ui->pRemainingTimeLbl->setText(msToTimecode(playlistRemainingMs, fps));

    if (m_currentIndex >= 0 && outMs > 0) {
      double clipRemainingMs = outMs - curMs;
      // 10 frame (00:00:00.10) kaldığında veya oynatıcı dosya sonuna
      // ulaştığında geçişi başlat
      double transitionThresholdMs = 10.0 * (1000.0 / fps);

      bool shouldTransition =
          (clipRemainingMs <= transitionThresholdMs && clipRemainingMs >= 0) ||
          m_lFile->isEOF();

      if (m_globalProps->currentState == globalProperties::Playing &&
          shouldTransition) {
        if (m_autoNextMedia) {
          int nextIndex = m_currentIndex + 1;
          if (nextIndex < m_playlistModel->rowCount()) {
            selectFile(nextIndex);
            if (m_nextMediaPause)
              QTimer::singleShot(50, this, &MainWindow::onPauseClicked);
          } else {
            if (m_playlistLoop && m_playlistModel->rowCount() > 0) {
              selectFile(0);
              if (m_nextMediaPause)
                QTimer::singleShot(50, this, &MainWindow::onPauseClicked);
            } else {
              onStopClicked();
            }
          }
          return;
        } else {
          onPauseClicked();
          return;
        }
      }

      ui->FileProgressBar->setMaximum(durMs > 0 ? static_cast<int>(durMs) : 1);
      ui->FileProgressBar->setValue(static_cast<int>(curMs));
      ui->FileProgressBar->setFormat(msToTimecode(curMs, fps) + " / " +
                                     msToTimecode(durMs, fps));
      if (QStandardItem *item = m_playlistModel->item(m_currentIndex, 1)) {
        ui->fileLabel->setText(item->text());
      }
      ui->clipremainingTimeLbl->setText(msToTimecode(durMs - curMs, fps));
    } else {
      ui->FileProgressBar->setMaximum(1);
      ui->FileProgressBar->setValue(0);
      ui->FileProgressBar->setFormat("00:00:00:00 / 00:00:00:00");
      if (m_currentIndex >= 0 && m_playlistModel->item(m_currentIndex, 1)) {
        ui->fileLabel->setText(
            m_playlistModel->item(m_currentIndex, 1)->text());
      } else {
        ui->fileLabel->setText("");
      }
      ui->clipremainingTimeLbl->setText("00:00:00:00");
    }
  } else {
    ui->FileProgressBar->setMaximum(1);
    ui->FileProgressBar->setValue(0);
    ui->FileProgressBar->setFormat("00:00:00:00 / 00:00:00:00");
    ui->PlaylistProgressBar->setMaximum(1);
    ui->PlaylistProgressBar->setValue(0);
    ui->PlaylistProgressBar->setFormat("00:00:00:00");
    ui->pCurTimeLbl->setText("00:00:00:00");
    ui->pDurTimeLbl->setText("00:00:00:00");
    ui->pRemainingTimeLbl->setText("00:00:00:00");
    ui->clipremainingTimeLbl->setText("00:00:00:00");
    ui->fileLabel->setText("");
  }

  // --- Oynatma Listesi Bitiş Zamanı (End Time) Hesaplaması ---
  static QDateTime frozenEndTime;
  static globalProperties::PlayerState lastState = globalProperties::Nothing;
  static double lastRemainingMs = 0;

  if (m_globalProps->currentState != globalProperties::Playing) {
    frozenEndTime = QDateTime::currentDateTime().addMSecs(
        static_cast<qint64>(playlistRemainingMs));
  } else {
    if (lastState != globalProperties::Playing ||
        std::abs(playlistRemainingMs - lastRemainingMs) > 2000) {
      frozenEndTime = QDateTime::currentDateTime().addMSecs(
          static_cast<qint64>(playlistRemainingMs));
    }
  }
  lastState = m_globalProps->currentState;
  lastRemainingMs = playlistRemainingMs;

  if (ui->pEndTimeLbl) {
    ui->pEndTimeLbl->setText(frozenEndTime.toString("HH:mm:ss"));
  }
  // -----------------------------------------------------------

  // Stream Stats Güncellemesi
  /*
  if (m_lStream) {
      QWidget* parent = ui->streamBtn->parentWidget();
      QProgressBar* pBar = parent->findChild<QProgressBar*>("streamBufferBar");
      QLabel* sLabel = parent->findChild<QLabel*>("streamSpeedLabel");

      if (pBar && sLabel && pBar->isVisible()) {
          LStream::Stats stats;
          m_lStream->getStats(stats);

          int queueSize = stats.bufferedFrames;
          pBar->setValue(queueSize);

          // Temel stil şablonu
          QString baseStyle = "QProgressBar { border: 1px solid grey;
  border-radius: 3px; text-align: center; color: white; font-weight: bold; }";

          if (queueSize < 30) {
              pBar->setStyleSheet(baseStyle + " QProgressBar::chunk {
  background-color: #50a050; }"); // Yeşil pBar->setFormat("Sağlık: Mükemmel");
          } else if (queueSize < 70) {
              pBar->setStyleSheet(baseStyle + " QProgressBar::chunk {
  background-color: #ffa500; }"); // Turuncu pBar->setFormat("Sağlık: Yoğun");
          } else {
              pBar->setStyleSheet(baseStyle + " QProgressBar::chunk {
  background-color: #ff4444; }"); // Kırmızı pBar->setFormat("Sağlık: Kritik");
          }

          sLabel->setText(QString("Speed: %1 Mbps").arg(stats.bitrateMbps, 0,
  'f', 2));
      }
  }
  */
  ui->playlistTableView->viewport()->update();
}

// ─── Sürükle-Bırak ──────────────────────────────────────────────────────────
bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == ui->playlistTableView->viewport() ||
      watched == ui->playlistTableView) {
    if (event->type() == QEvent::DragEnter) {
      auto *dee = static_cast<QDragEnterEvent *>(event);
      if (dee->mimeData()->hasUrls() ||
          dee->mimeData()->hasFormat(
              "application/x-qabstractitemmodeldatalist")) {
        dee->acceptProposedAction();
        return true;
      }
      return false;
    } else if (event->type() == QEvent::DragMove) {
      auto *dme = static_cast<QDragMoveEvent *>(event);
      if (dme->mimeData()->hasUrls() ||
          dme->mimeData()->hasFormat(
              "application/x-qabstractitemmodeldatalist")) {
        QModelIndex idx =
            ui->playlistTableView->indexAt(dme->position().toPoint());
        int newRow = m_playlistModel->rowCount();
        if (idx.isValid()) {
          QRect rect = ui->playlistTableView->visualRect(idx);
          if (dme->position().y() < rect.center().y()) {
            newRow = idx.row();
          } else {
            newRow = idx.row() + 1;
          }
        }
        if (newRow != m_globalProps->dropIndicatorRow) {
          m_globalProps->dropIndicatorRow = newRow;
          ui->playlistTableView->viewport()->update();
        }
        dme->acceptProposedAction();
        return true;
      }
      return false;
    } else if (event->type() == QEvent::DragLeave) {
      m_globalProps->dropIndicatorRow = -1;
      ui->playlistTableView->viewport()->update();
      return false;
    } else if (event->type() == QEvent::Drop) {
      auto *de = static_cast<QDropEvent *>(event);
      m_globalProps->dropIndicatorRow = -1;
      ui->playlistTableView->viewport()->update();
      const QMimeData *md = de->mimeData();
      if (md->hasUrls()) {
        QModelIndex idx =
            ui->playlistTableView->indexAt(de->position().toPoint());
        int dropRow = m_playlistModel->rowCount();
        if (idx.isValid()) {
          QRect rect = ui->playlistTableView->visualRect(idx);
          if (de->position().y() < rect.center().y()) {
            dropRow = idx.row();
          } else {
            dropRow = idx.row() + 1;
          }
        }
        for (const QUrl &url : md->urls()) {
          QString fp = url.toLocalFile();
          if (!fp.isEmpty()) {
            addFileToPlaylist(fp, dropRow);
            dropRow++;
          }
        }
        de->acceptProposedAction();
        return true;
      } else if (md->hasFormat("application/x-qabstractitemmodeldatalist")) {
        QModelIndexList selectedRows =
            ui->playlistTableView->selectionModel()->selectedRows();
        if (!selectedRows.isEmpty()) {
          int sourceRow = selectedRows.first().row();
          QModelIndex idx =
              ui->playlistTableView->indexAt(de->position().toPoint());
          int dropRow = m_playlistModel->rowCount();
          if (idx.isValid()) {
            QRect rect = ui->playlistTableView->visualRect(idx);
            if (de->position().y() < rect.center().y()) {
              dropRow = idx.row();
            } else {
              dropRow = idx.row() + 1;
            }
          }

          if (sourceRow != dropRow && sourceRow != dropRow - 1) {
            QList<QStandardItem *> items = m_playlistModel->takeRow(sourceRow);
            if (dropRow > sourceRow)
              dropRow--;

            m_playlistModel->insertRow(dropRow, items);

            for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
              m_playlistModel->item(i, 0)->setText(QString::number(i + 1));
            }

            int oldIndex = m_currentIndex;
            if (sourceRow == oldIndex) {
              m_currentIndex = dropRow;
            } else if (sourceRow < oldIndex && dropRow >= oldIndex) {
              m_currentIndex--;
            } else if (sourceRow > oldIndex && dropRow <= oldIndex) {
              m_currentIndex++;
            }

            ui->playlistTableView->selectRow(dropRow);
          }
        }
        de->setDropAction(Qt::IgnoreAction);
        de->accept();
        return true;
      }
      return false;
    } else if (event->type() == QEvent::KeyPress) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
      if (keyEvent->key() == Qt::Key_Delete) {
        deleteSelectedFiles();
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

// ─── Disk Gezgini ───────────────────────────────────────────────────────────
void MainWindow::onDriveChanged(int index) {
  QString path = ui->driveComboBox->itemData(index).toString();
  if (!path.isEmpty())
    ui->listViewExp->setRootIndex(m_listModel->setRootPath(path));
}

void MainWindow::onExplorerActivated(const QModelIndex &index) {
  if (m_listModel->isDir(index))
    ui->listViewExp->setRootIndex(
        m_listModel->setRootPath(m_listModel->filePath(index)));
}

void MainWindow::refreshDrives() {
  QString currentPath = ui->driveComboBox->currentData().toString();
  QStringList newPaths;
  QList<QPair<QString, QString>> driveList;

  for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
    if (storage.isValid() && storage.isReady() && !storage.isReadOnly()) {
      QString path = storage.rootPath();
      bool ok = (path == "/") || (path == "/home") ||
                path.startsWith("/media") || path.startsWith("/mnt") ||
                path.startsWith("/run/media");
      if (ok) {
        QString name = storage.displayName();
        if (path == "/")
          name = "Sistem (Root)";
        else if (path == "/home")
          name = "Kullanıcılar (Home)";
        else if (name.isEmpty())
          name = path;
        driveList.append({name, path});
        newPaths << path;
      }
    }
  }

  static QStringList lastPaths;
  if (newPaths == lastPaths)
    return;
  lastPaths = newPaths;

  ui->driveComboBox->blockSignals(true);
  ui->driveComboBox->clear();
  int toSelect = 0;
  for (int i = 0; i < driveList.size(); ++i) {
    ui->driveComboBox->addItem(driveList[i].first, driveList[i].second);
    if (driveList[i].second == currentPath)
      toSelect = i;
  }
  ui->driveComboBox->setCurrentIndex(toSelect);
  ui->driveComboBox->blockSignals(false);
}



void MainWindow::onPlaylistContextMenuRequested(const QPoint &pos) {
  QModelIndexList selectedRows =
      ui->playlistTableView->selectionModel()->selectedRows();
  int targetRow = m_playlistModel->rowCount();
  if (!selectedRows.isEmpty()) {
    targetRow = selectedRows.last().row() + 1; // Seçili olanın bir altına ekle
  }

  QMenu menu(this);
  QAction *actClipTrim = menu.addAction("Clip Trim...");
  menu.addSeparator();

  QAction *actAddFile = menu.addAction("Add File");
  QAction *actInsertFile = menu.addAction("Insert File");
  QAction *actAddPlaylist = menu.addAction("Add Playlist");
  QAction *actInsertPlaylist = menu.addAction("Insert Playlist");
  QAction *actDeleteFile = menu.addAction("Delete File(s)");

  menu.addSeparator();

  QMenu *cmdMenu = menu.addMenu("Add Command   ");
  QAction *actLoop = cmdMenu->addAction("Loop (This Video)");
  QAction *actHold = cmdMenu->addAction("Pause (Hold)");
  QAction *actStop = cmdMenu->addAction("Stop");
  QAction *actPlaylistLoop = cmdMenu->addAction("Playlist Loop (Back to Top)");

  QAction *selectedAct =
      menu.exec(ui->playlistTableView->viewport()->mapToGlobal(pos));
  if (!selectedAct)
    return;

  if (selectedAct == actClipTrim) {
    int selectedIdx = selectedRows.last().row();
    QString pathOrCmd =
        m_playlistModel->item(selectedIdx, 0)->data(Qt::UserRole).toString();
    if (!pathOrCmd.startsWith("CMD:")) {
      QString path = pathOrCmd;
      double inMs = 0;
      double outMs = 0;
      QStringList parts = pathOrCmd.split("|");
      if (parts.size() >= 3) {
        path = parts[0];
        inMs = parts[1].toDouble();
        outMs = parts[2].toDouble();
      } else {
        QFileInfo fi(path);
        LReader probe;
        if (probe.open(path.toStdString())) {
          outMs = probe.getDurationMs();
        }
      }

      TrimDialog dlg(this);
      dlg.initFile(path, inMs, outMs);

      double fps = 25.0;
      LReader probe;
      if (probe.open(path.toStdString()))
        fps = probe.getFps();

      if (dlg.exec() == QDialog::Accepted) {
        double newInMs = timecodeToMs(dlg.getInTime(), fps);
        double newOutMs = timecodeToMs(dlg.getOutTime(), fps);

        QString newData =
            QString("%1|%2|%3").arg(path).arg(newInMs).arg(newOutMs);

        m_playlistModel->item(selectedIdx, 0)->setData(newData, Qt::UserRole);
        m_playlistModel->item(selectedIdx, 2)
            ->setText(msToTimecode(newInMs, fps));
        m_playlistModel->item(selectedIdx, 3)
            ->setText(msToTimecode(newOutMs, fps));
        m_playlistModel->item(selectedIdx, 4)
            ->setText(msToTimecode(newOutMs - newInMs, fps));
      }
    }
  } else if (selectedAct == actAddFile) {
    onAddFile();
  } else if (selectedAct == actInsertFile) {
    FileDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
      for (const QString &fp : dlg.selectedFiles()) {
        addFileToPlaylist(fp, targetRow);
        targetRow++; // Increment so multiple files insert sequentially
      }
    }
  } else if (selectedAct == actAddPlaylist) {
    QString fileName = QFileDialog::getOpenFileName(
        this, "Add Playlist", "", "now2play Playlist (*.nplist)");
    if (!fileName.isEmpty())
      loadPlaylistFile(fileName, false, -1);
  } else if (selectedAct == actInsertPlaylist) {
    QString fileName = QFileDialog::getOpenFileName(
        this, "Insert Playlist", "", "now2play Playlist (*.nplist)");
    if (!fileName.isEmpty())
      loadPlaylistFile(fileName, false, targetRow);
  } else if (selectedAct == actDeleteFile) {
    deleteSelectedFiles();
  } else if (selectedAct == actLoop) {
    addCommandToPlaylist("LOOP", targetRow);
  } else if (selectedAct == actHold) {
    addCommandToPlaylist("PAUSE", targetRow);
  } else if (selectedAct == actStop) {
    addCommandToPlaylist("STOP", targetRow);
  } else if (selectedAct == actPlaylistLoop) {
    addCommandToPlaylist("PLAYLIST_LOOP", targetRow);
  }
}

void MainWindow::addCommandToPlaylist(const QString &cmdType, int row) {
  int finalRow = (row < 0 || row > m_playlistModel->rowCount())
                     ? m_playlistModel->rowCount()
                     : row;

  QString cmdStr = "CMD:" + cmdType;
  QString displayCmd = "[command] - " + cmdType;

  auto *posItem = new QStandardItem(QString::number(finalRow + 1));
  posItem->setData(cmdStr, Qt::UserRole);

  QList<QStandardItem *> rowItems = {posItem,
                                     new QStandardItem(displayCmd),
                                     new QStandardItem("-"),
                                     new QStandardItem("-"),
                                     new QStandardItem("-"),
                                     new QStandardItem("-")};
  m_playlistModel->insertRow(finalRow, rowItems);

  for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
    if (auto *it = m_playlistModel->item(i, 0))
      it->setText(QString::number(i + 1));
  }
  updateButtonStates();
  ui->playlistTableView->viewport()->update();
}

void MainWindow::deleteSelectedFiles() {
  QModelIndexList selectedRows =
      ui->playlistTableView->selectionModel()->selectedRows();
  if (selectedRows.isEmpty())
    return;

  std::sort(selectedRows.begin(), selectedRows.end(),
            [](const QModelIndex &a, const QModelIndex &b) {
              return a.row() > b.row();
            });

  bool cantDeletePlaying = false;
  for (const QModelIndex &idx : selectedRows) {
    if (idx.row() == m_currentIndex) {
      cantDeletePlaying = true;
      break;
    }
  }

  if (cantDeletePlaying) {
    QMessageBox::warning(this, "Action Denied",
                         "This video is currently loaded in the player or "
                         "playing!\nPlease stop the video before deleting it "
                         "(switch to a different video or clear the list).");
    return;
  }

  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(
      this, "Delete",
      "Are you sure you want to delete the selected items from the list?",
      QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    for (const QModelIndex &idx : selectedRows) {
      m_playlistModel->removeRow(idx.row());
      if (idx.row() < m_currentIndex) {
        m_currentIndex--;
      }
    }

    for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
      if (auto *it = m_playlistModel->item(i, 0))
        it->setText(QString::number(i + 1));
    }

    if (m_playlistModel->rowCount() == 0) {
      m_currentIndex = -1;
    }
    updateButtonStates();
  }
}

void MainWindow::on_deinterlace_changed(int state) {
  bool enabled = (state == Qt::Checked);
  m_deinterlaceEnabled = enabled;
  if (m_lFile)
    m_lFile->setProps("deinterlace", enabled ? "true" : "false");
}

void MainWindow::on_scale_type_changed(const QString &text) {
  m_scaleType = text;
  if (m_lFile)
    m_lFile->setProps("scale_type", text.toStdString());
}

void MainWindow::on_gpu_decoding_changed(int state) {
  bool enabled = (state == Qt::Checked);
  m_gpuDecoding = enabled;
  if (m_lFile)
    m_lFile->setProps("gpu", enabled ? "true" : "false");
}
