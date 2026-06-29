#include "splashwidget.h"
#include "ui_splashwidget.h"

SplashWidget::SplashWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SplashWidget)
{
    ui->setupUi(this);

    // Progress bar için istenen renk: #344871
    // Fusion stilindeki indeterminate animasyonunu bozmamak için renk paletini güncelliyoruz
    QPalette p = ui->progressBar->palette();
    p.setColor(QPalette::Highlight, QColor("#344871"));
    ui->progressBar->setPalette(p);

    // Versiyonu CMakeLists.txt'ten (APP_VERSION) dinamik olarak alıp etikete yazdırıyoruz
#ifdef APP_VERSION
    ui->versionLabel->setText(QString("v%1").arg(APP_VERSION));
#else
    ui->versionLabel->setText("vUnknown");
#endif
}

SplashWidget::~SplashWidget()
{
    delete ui;
}
