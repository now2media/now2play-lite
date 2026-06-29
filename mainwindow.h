#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFileSystemModel>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTimer>
#include <QWidget>

#include "LFile.h"
#include "LPreview.h"
#include "LMixer.h"
#include "LReader.h"
#include "LOutput.h"
#include "LCharacter.h"
#include "globalproperties.h"
#include "settingsdialog.h"
#include <QSettings>

class LCharacter;
class LCGTextItem;
class LCGTickerItem;
class LCGImageItem;
class LCGRectItem;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class LSignal;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;
  LCGImageProps mainLogoProps;
  LCGImageItem* mainLogo = nullptr;

private slots:
  void onAddFile();
  void addFileToPlaylist(const QString &filePath, int row = -1);
  void selectFile(int index);
  void onRowDoubleClicked(const QModelIndex &index);
  void onPlayClicked();
  void onPauseClicked();
  void onStopClicked();
  void updateUI();
  void updateButtonStates();
  void onOutDeviceChanged(int index);
  void onOutChannelChanged(int index);
  void onOutFormatChanged(int index);
  void onOutAudioFormatChanged(int index);
  void refreshOutputSources();
  void onDriveChanged(int index);
  void onExplorerActivated(const QModelIndex &index);
  void refreshDrives();
  void onSettingsClicked();
  void onNextClicked();
  void onBackClicked();
  void onMutePreviewClicked();
  void onEmptyPlaylistClicked();
  void onSavePlaylistClicked();
  void onOpenPlaylistClicked();
  void onAddPlaylistClicked();
  void onPlaylistContextMenuRequested(const QPoint &pos);
  void addCommandToPlaylist(const QString &cmdType, int row = -1);
  void loadPlaylistFile(const QString &fileName, bool clearFirst, int targetRow = -1);
  void deleteSelectedFiles();
  void loadSettings();
  void on_deinterlace_changed(int state);
  void on_scale_type_changed(const QString &text);
  void on_gpu_decoding_changed(int state);

  // --- Oynatma / Liste Takip ---
  void saveSettings();

  private:
  Ui::MainWindow *ui;
  LLayerItem* m_selectedLayer = nullptr;
  LFilter* colorFilter;
  LFilter* delayfilter;

  // --- Playlist ---
  QStandardItemModel *m_playlistModel;

  // --- LinuxMediaLibrary çekirdeği ---
  LFile    *m_lFile         = nullptr;

  LReader    *m_lReader         = nullptr;
  LPreview *m_lPreviewLeft  = nullptr;  // playerPreview (sol)
  LPreview *m_lPreviewMixer = nullptr;
  LMixer   *m_lMixer        = nullptr;
  LCharacter *m_lCG           = nullptr;
  LSignal   *mixerBg         = nullptr;
  LObject   *m_prevObj       = nullptr;
  LOutput   *m_lOutput       = nullptr;
  class QLabel    *m_testLabel     = nullptr;
  QTimer    *m_testTimer     = nullptr;
  class QAudioSink *m_audioSink    = nullptr;
  class QIODevice  *m_audioIoDevice = nullptr;

  // --- Durum ---
  globalProperties *m_globalProps;
  int    m_currentIndex  = -1;
  int    m_elapsedFrames = 0;
  double m_currentFps    = 25.0;

  // --- Dosya Gezgini ---
  QFileSystemModel *m_treeModel;
  QFileSystemModel *m_listModel;
  bool eventFilter(QObject *watched, QEvent *event) override;

  QTimer *m_uiTimer;
  SettingsDialog *m_settingsDialog = nullptr;
  QSettings *m_settings = nullptr;

  bool m_autoNextMedia = true;
  bool m_playlistLoop = false;
  bool m_nextMediaPause = false;
  bool m_previewMuted = true;
  bool m_outputEnabled = false;
  bool m_deinterlaceEnabled = true;
  QString m_scaleType = "aspect-ratio";
  bool m_gpuDecoding = false;
};

#endif // MAINWINDOW_H
