#include "mainwindow.h"
#include "splashwidget.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QIcon>
#include <QPalette>
#include <QStyleFactory>
#include <QSurfaceFormat>

int main(int argc, char *argv[]) {
  // --- YAZILIMSAL ZIRH: Donanım ve Sürücü Uyumluluğu ---

  // Qt RHI Katmanını OpenGL'e sabitle (AMD/Wayland/X11 uyumluluğu için)
  qputenv("QSG_RHI_BACKEND", "opengl");

  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  qputenv("QSG_RHI_BACKEND", "opengl");
  // qputenv("QT_XCB_GL_INTEGRATION", "xcb_egl"); // Intel kartlarda siyah ekran
  // yapiyor olabilir, iptal edildi.

  // Kaynak paylaşımını aç (Threadler arası GL context paylaşımı)
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(":/now2play.png"));

  // --- PROFESYONEL YAYIN (PLAYOUT) DARK TEMASI ---
  a.setStyle(QStyleFactory::create("Fusion"));
  QPalette darkPalette;
  darkPalette.setColor(QPalette::Window, QColor(45, 45, 45));
  darkPalette.setColor(QPalette::WindowText, Qt::white);
  darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
  darkPalette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
  darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
  darkPalette.setColor(QPalette::ToolTipText, Qt::white);
  darkPalette.setColor(QPalette::Text, Qt::white);
  darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::ButtonText, Qt::white);
  darkPalette.setColor(QPalette::BrightText, Qt::red);
  darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::HighlightedText, Qt::black);
  a.setPalette(darkPalette);
  a.setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; "
                  "border: 1px solid white; }");
  // ------------------------------------------------

  // Açılış ekranını göster (Çerçevesiz ve her zaman üstte)
  SplashWidget *splash = new SplashWidget();
  splash->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  splash->show();
  QApplication::processEvents(); // Ekranın çizilmesini bekle

  // Çok hızlı açıldığı için ekranda 2 saniye kalmasını sağlayan akıllı bekleme döngüsü
  QElapsedTimer timer;
  timer.start();
  while(timer.elapsed() < 2000) {
      QApplication::processEvents();
  }

  MainWindow w;
  w.show();
  
  // Ana pencere açılınca açılış ekranını kapat
  splash->close();
  splash->deleteLater();

  int result = a.exec();

  return result;
}