#pragma once

#include "globalproperties.h" // State tanımı için gerekli
#include <QImage>
#include <QMutex>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <algorithm>

class MasterWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT
public:
  explicit MasterWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
    // Qt'nin kendi boyama sistemini devre dışı bırakıyoruz (Beyazlık önleyici)
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setStyleSheet("background-color:black;");
  }

  ~MasterWidget() {
    if (m_textureId != 0) {
      makeCurrent();
      glDeleteTextures(1, &m_textureId);
      doneCurrent();
    }
  }

  // MainWindow'dan gelen merkezi özellikleri bağlar
  void setGlobalProps(globalProperties *props) { m_props = props; }

public slots:
  void showFrame(const QImage &img, float volL = 0.0f, float volR = 0.0f) {
    QMutexLocker lk(&m_mutex);
    m_frame = img;
    m_vuLevelL = volL;
    m_vuLevelR = volR;

    // Peak Hold Hesaplama
    if (volL >= m_peakHoldL) {
      m_peakHoldL = volL;
      m_peakTimerL = 45;
    } else {
      if (m_peakTimerL > 0)
        m_peakTimerL--;
      else
        m_peakHoldL -= 0.006f;
    }

    if (volR >= m_peakHoldR) {
      m_peakHoldR = volR;
      m_peakTimerR = 45;
    } else {
      if (m_peakTimerR > 0)
        m_peakTimerR--;
      else
        m_peakHoldR -= 0.006f;
    }

    m_peakHoldL = std::max(0.0f, m_peakHoldL);
    m_peakHoldR = std::max(0.0f, m_peakHoldR);
    update();
  }

protected:
  void initializeGL() override {
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  void paintGL() override {
    // 1. Her zaman ekranı SİYAH temizle
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_props)
      return;

    QMutexLocker lk(&m_mutex);

    // 2. State Kontrolü: Nothing ise SİYAH, diğer durumlarda BEYAZ fırça
    if (m_props->currentState == globalProperties::Nothing) {
      glColor4f(0.0f, 0.0f, 0.0f,
                1.0f); // Hiçbir şey yoksa pikselleri sıfırla çarp (Siyah)
    } else {
      glColor4f(1.0f, 1.0f, 1.0f,
                1.0f); // Play/Pause/Stop ise pikselleri 1 ile çarp (Orijinal)
    }

    // 3. Video Çizimi (Sadece Nothing değilse ve geçerli kare varsa)
    if (m_props->currentState != globalProperties::Nothing &&
        !m_frame.isNull()) {
      glBindTexture(GL_TEXTURE_2D, m_textureId);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_frame.width(), m_frame.height(),
                   0, GL_RGB, GL_UNSIGNED_BYTE, m_frame.bits());

      glEnable(GL_TEXTURE_2D);
      glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 1.0f);
      glVertex2f(-1.0f, -1.0f);
      glTexCoord2f(1.0f, 1.0f);
      glVertex2f(1.0f, -1.0f);
      glTexCoord2f(1.0f, 0.0f);
      glVertex2f(1.0f, 1.0f);
      glTexCoord2f(0.0f, 0.0f);
      glVertex2f(-1.0f, 1.0f);
      glEnd();
      glDisable(GL_TEXTURE_2D);
    }

    // 4. VU Metre Çizimi (Her zaman videonun üstünde)
    // renderVUMeter();
  }

  void resizeGL(int w, int h) override { glViewport(0, 0, w, h); }

private:
  void renderVUMeter() {
    const float pxW = 2.0f / (float)width();
    const float pxH = 2.0f / (float)height();

    float padX = 10.0f * pxW;
    float padY = 10.0f * pxH;
    float innerPad = 5.0f * pxH;
    float segGap = 2.0f * pxH;

    const float panelW = 0.12f;
    const float barW = 0.038f;
    const float gapLR = 0.01f;
    const int segments = 40;

    const float startX = 1.0f - panelW - padX;
    const float startY = -1.0f + padY;
    const float totalH = 2.0f - (padY * 2.0f);

    // Arka Plan Paneli (Yarı şeffaf siyah)
    glEnable(GL_BLEND);
    // 2. Renklerin nasıl karıştırılacağını belirle (Standart Alpha blending)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
    glBegin(GL_QUADS);
    glVertex2f(startX, startY);
    glVertex2f(startX + panelW, startY);
    glVertex2f(startX + panelW, startY + totalH);
    glVertex2f(startX, startY + totalH);
    glEnd();
    glDisable(GL_BLEND);

    float meterH = totalH - (innerPad * 2.0f);
    float segH = (meterH - (segments - 1) * segGap) / (float)segments;

    auto drawBarsWithPeak = [&](float level, float peakLevel, float xPos) {
      for (int i = 0; i < segments; ++i) {
        float yPos = startY + innerPad + i * (segH + segGap);
        float r, g, b;
        float ratio = (float)i / segments;

        if (ratio < 0.65f) {
          r = 0.0f;
          g = 0.8f;
          b = 0.0f;
        } else if (ratio < 0.88f) {
          r = 0.9f;
          g = 0.8f;
          b = 0.0f;
        } else {
          r = 1.0f;
          g = 0.0f;
          b = 0.0f;
        }

        if (i < (int)(level * segments))
          glColor4f(r, g, b, 1.0f);
        else
          glColor4f(r * 0.12f, g * 0.12f, b * 0.12f, 0.4f);

        glBegin(GL_QUADS);
        glVertex2f(xPos, yPos);
        glVertex2f(xPos + barW, yPos);
        glVertex2f(xPos + barW, yPos + segH);
        glVertex2f(xPos, yPos + segH);
        glEnd();
      }

      // Peak Çizgisi
      if (peakLevel > 0.01f) {
        int pIdx = (int)(peakLevel * segments);
        if (pIdx >= segments)
          pIdx = segments - 1;
        float pyPos = startY + innerPad + pIdx * (segH + segGap);
        float pr, pg, pb;
        float pRatio = (float)pIdx / segments;

        if (pRatio < 0.65f) {
          pr = 0.0f;
          pg = 0.9f;
          pb = 0.0f;
        } else if (pRatio < 0.88f) {
          pr = 1.0f;
          pg = 1.0f;
          pb = 0.0f;
        } else {
          pr = 1.0f;
          pg = 0.0f;
          pb = 0.0f;
        }

        glColor4f(pr, pg, pb, 0.9f);
        glBegin(GL_QUADS);
        glVertex2f(xPos, pyPos + segH - (2.0f * pxH));
        glVertex2f(xPos + barW, pyPos + segH - (2.0f * pxH));
        glVertex2f(xPos + barW, pyPos + segH);
        glVertex2f(xPos, pyPos + segH);
        glEnd();
      }
    };

    float bStartX = startX + (panelW - (barW * 2 + gapLR)) / 2.0f;
    drawBarsWithPeak(m_vuLevelL, m_peakHoldL, bStartX);
    drawBarsWithPeak(m_vuLevelR, m_peakHoldR, bStartX + barW + gapLR);
  }

  QImage m_frame;
  QMutex m_mutex;
  GLuint m_textureId{0};
  globalProperties *m_props = nullptr; // Merkezi state nesnesi
  float m_vuLevelL{0.0f}, m_vuLevelR{0.0f};
  float m_peakHoldL{0.0f}, m_peakHoldR{0.0f};
  int m_peakTimerL{0}, m_peakTimerR{0};
};