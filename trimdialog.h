#ifndef TRIMDIALOG_H
#define TRIMDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QMouseEvent>
#include "LFile.h"
#include "LPreview.h"

namespace Ui {
class TrimDialog;
}

class TrimDialog : public QDialog {
  Q_OBJECT

public:
  explicit TrimDialog(QWidget *parent = nullptr);
  ~TrimDialog();

  void initFile(const QString &path, double inMs, double outMs);

  QString getInTime() const;
  QString getOutTime() const;

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onLowerScrubbed(int val);
  void onUpperScrubbed(int val);
  
  void onMarkIn();
  void onMarkOut();
  void onSetIn();
  void onSetOut();
  
  void onPlay();
  void onPause();
  
  void onFrameBack();
  void onFrameFwd();
  void onSecBack();
  void onSecFwd();
  void onMinBack();
  void onMinFwd();
  
  void onAudioToggle();
  void onApply();
  void onCancel();
  
  void updateProgress();

private:
  Ui::TrimDialog *ui;
  LFile *m_file;
  LPreview *m_preview;
  QTimer *m_timer;
  
  double m_fps;
  double m_durationMs;
  bool m_audioEnabled;
  bool m_isScrubbing;

  QString msToTimecode(double ms);
  double timecodeToMs(const QString &tc);
  void stepByMs(double msAmount);
};

#endif // TRIMDIALOG_H
