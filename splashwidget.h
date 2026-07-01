#ifndef SPLASHWIDGET_H
#define SPLASHWIDGET_H

#include <QWidget>

namespace Ui {
class SplashWidget;
}

class SplashWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SplashWidget(QWidget *parent = nullptr);
    ~SplashWidget();

private:
    Ui::SplashWidget *ui;
};

#endif // SPLASHWIDGET_H
