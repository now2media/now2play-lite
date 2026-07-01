#include "trimdialog.h"
#include "ui_trimdialog.h"
#include <QFileInfo>
#include "LReader.h"
#include <QDebug>

TrimDialog::TrimDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::TrimDialog), m_file(new LFile()),
      m_preview(new LPreview()), m_timer(new QTimer(this)),
      m_fps(25.0), m_durationMs(0), m_audioEnabled(false), m_isScrubbing(false) {
  ui->setupUi(this);

  // Set resource SVG icons for dialog buttons
  ui->btnPlay->setIcon(QIcon(":/icons/play.svg"));
  ui->btnPause->setIcon(QIcon(":/icons/pause.svg"));
  ui->btnAudioToggle->setIcon(QIcon(":/icons/mute.svg")); // Initially muted

  m_preview->setProps("ui_framework", "qt");
  m_preview->previewEnable(ui->wPreview, false, true);
  m_preview->previewObject(m_file);

  ui->scrubBar->installEventFilter(this);

  connect(ui->rangeSlider, &RangeSlider::lowerPositionChanged, this, &TrimDialog::onLowerScrubbed);
  connect(ui->rangeSlider, &RangeSlider::upperPositionChanged, this, &TrimDialog::onUpperScrubbed);
  
  connect(ui->btnMarkIn, &QPushButton::clicked, this, &TrimDialog::onMarkIn);
  connect(ui->btnMarkOut, &QPushButton::clicked, this, &TrimDialog::onMarkOut);
  connect(ui->btnSetIn, &QPushButton::clicked, this, &TrimDialog::onSetIn);
  connect(ui->btnSetOut, &QPushButton::clicked, this, &TrimDialog::onSetOut);
  
  connect(ui->btnPlay, &QPushButton::clicked, this, &TrimDialog::onPlay);
  connect(ui->btnPause, &QPushButton::clicked, this, &TrimDialog::onPause);
  
  connect(ui->btnFrameBack, &QPushButton::clicked, this, &TrimDialog::onFrameBack);
  connect(ui->btnFrameFwd, &QPushButton::clicked, this, &TrimDialog::onFrameFwd);
  connect(ui->btnSecBack, &QPushButton::clicked, this, &TrimDialog::onSecBack);
  connect(ui->btnSecFwd, &QPushButton::clicked, this, &TrimDialog::onSecFwd);
  connect(ui->btnMinBack, &QPushButton::clicked, this, &TrimDialog::onMinBack);
  connect(ui->btnMinFwd, &QPushButton::clicked, this, &TrimDialog::onMinFwd);
  
  connect(ui->btnAudioToggle, &QPushButton::clicked, this, &TrimDialog::onAudioToggle);
  connect(ui->btnApply, &QPushButton::clicked, this, &TrimDialog::onApply);
  connect(ui->btnCancel, &QPushButton::clicked, this, &TrimDialog::onCancel);

  connect(m_timer, &QTimer::timeout, this, &TrimDialog::updateProgress);
  m_timer->start(30);
  
  // Audio VU Meter is off initially
  m_preview->setProps("audio", "false");
  m_preview->setProps("audio_meter", "true"); // Always calculate VU meter
}

TrimDialog::~TrimDialog() {
  m_timer->stop();
  m_file->stop();
  delete m_preview;
  delete m_file;
  delete ui;
}

QString TrimDialog::msToTimecode(double ms) {
  if (ms < 0) ms = 0;
  int total_f = qRound((ms * m_fps) / 1000.0);
  int ff = total_f % static_cast<int>(m_fps);
  int total_s = total_f / static_cast<int>(m_fps);
  int ss = total_s % 60;
  int mm = (total_s / 60) % 60;
  int hh = total_s / 3600;
  return QString::asprintf("%02d:%02d:%02d.%02d", hh, mm, ss, ff);
}

double TrimDialog::timecodeToMs(const QString &tc) {
    int hh = 0, mm = 0, ss = 0, ff = 0;
    sscanf(tc.toStdString().c_str(), "%d:%d:%d.%d", &hh, &mm, &ss, &ff);
    return (hh * 3600 + mm * 60 + ss) * 1000.0 + (ff * 1000.0 / m_fps);
}

void TrimDialog::initFile(const QString &path, double inMs, double outMs) {
  QFileInfo fi(path);
  setWindowTitle("Next4Play Clip Trim - " + fi.fileName());

  LReader probe;
  if (probe.open(path.toStdString())) {
      m_durationMs = probe.getDurationMs();
      m_fps = probe.getFps();
  }
  if (m_fps <= 0) m_fps = 25.0;

  m_file->fileNameSet(path.toStdString());

  ui->rangeSlider->setMinimum(0);
  ui->rangeSlider->setMaximum(m_durationMs);
  ui->scrubBar->setMaximum(m_durationMs);
  
  if (outMs <= 0 || outMs > m_durationMs) outMs = m_durationMs;
  
  ui->rangeSlider->setUpperValue(m_durationMs);
  ui->rangeSlider->setLowerValue(inMs);
  ui->rangeSlider->setUpperValue(outMs);

  ui->leInTime->setText(msToTimecode(inMs));
  ui->leOutTime->setText(msToTimecode(outMs));

  m_file->PosSet(inMs);
  m_file->play();
  m_file->pause();
}

bool TrimDialog::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->scrubBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_isScrubbing = true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_isScrubbing = false;
        }

        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                double ratio = (double)mouseEvent->pos().x() / ui->scrubBar->width();
                double targetMs = ratio * m_durationMs;
                if (targetMs < 0) targetMs = 0;
                if (targetMs > m_durationMs) targetMs = m_durationMs;
                
                m_file->PosSet(targetMs);
                ui->scrubBar->setValue(targetMs);
                ui->scrubBar->setFormat(msToTimecode(targetMs));
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void TrimDialog::updateProgress() {
    if (!m_isScrubbing) {
        double curMs = m_file->PosGet();
        ui->scrubBar->setValue(curMs);
        ui->scrubBar->setFormat(msToTimecode(curMs));
    }
}

void TrimDialog::onLowerScrubbed(int val) {
  m_file->PosSet(val);
  ui->leInTime->setText(msToTimecode(val));
}

void TrimDialog::onUpperScrubbed(int val) {
  m_file->PosSet(val);
  ui->leOutTime->setText(msToTimecode(val));
}

void TrimDialog::onMarkIn() {
    double curMs = m_file->PosGet();
    ui->leInTime->setText(msToTimecode(curMs));
    ui->rangeSlider->setLowerValue(curMs);
}

void TrimDialog::onMarkOut() {
    double curMs = m_file->PosGet();
    ui->leOutTime->setText(msToTimecode(curMs));
    ui->rangeSlider->setUpperValue(curMs);
}

void TrimDialog::onSetIn() {
    double inMs = timecodeToMs(ui->leInTime->text());
    ui->rangeSlider->setLowerValue(inMs);
}

void TrimDialog::onSetOut() {
    double outMs = timecodeToMs(ui->leOutTime->text());
    ui->rangeSlider->setUpperValue(outMs);
}

void TrimDialog::onPlay() {
    m_file->play();
}

void TrimDialog::onPause() {
    m_file->pause();
}

void TrimDialog::stepByMs(double msAmount) {
    double curMs = m_file->PosGet();
    double targetMs = curMs + msAmount;
    if (targetMs < 0) targetMs = 0;
    if (targetMs > m_durationMs) targetMs = m_durationMs;
    m_file->PosSet(targetMs);
}

void TrimDialog::onFrameBack() { stepByMs(-1000.0 / m_fps); }
void TrimDialog::onFrameFwd() { stepByMs(1000.0 / m_fps); }
void TrimDialog::onSecBack() { stepByMs(-1000.0); }
void TrimDialog::onSecFwd() { stepByMs(1000.0); }
void TrimDialog::onMinBack() { stepByMs(-60000.0); }
void TrimDialog::onMinFwd() { stepByMs(60000.0); }

void TrimDialog::onAudioToggle() {
    m_audioEnabled = !m_audioEnabled;
    m_preview->setProps("audio", m_audioEnabled ? "true" : "false");
    ui->btnAudioToggle->setIcon(QIcon(m_audioEnabled ? ":/icons/volume.svg" : ":/icons/mute.svg"));
}

void TrimDialog::onApply() {
    accept();
}

void TrimDialog::onCancel() {
    reject();
}

QString TrimDialog::getInTime() const {
  return ui->leInTime->text();
}

QString TrimDialog::getOutTime() const {
  return ui->leOutTime->text();
}
