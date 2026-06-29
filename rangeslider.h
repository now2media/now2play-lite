#ifndef RANGESLIDER_H
#define RANGESLIDER_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class RangeSlider : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int lowerValue READ lowerValue WRITE setLowerValue)
    Q_PROPERTY(int upperValue READ upperValue WRITE setUpperValue)

public:
    explicit RangeSlider(QWidget *parent = nullptr);

    int minimum() const { return m_minimum; }
    void setMinimum(int min);

    int maximum() const { return m_maximum; }
    void setMaximum(int max);

    int lowerValue() const { return m_lowerValue; }
    int upperValue() const { return m_upperValue; }

public slots:
    void setLowerValue(int val);
    void setUpperValue(int val);

signals:
    void lowerValueChanged(int val);
    void upperValueChanged(int val);
    void lowerPositionChanged(int val); // Sürüklenirken
    void upperPositionChanged(int val); // Sürüklenirken

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_minimum = 0;
    int m_maximum = 100;
    int m_lowerValue = 0;
    int m_upperValue = 100;

    enum ActiveSlider { None, Lower, Upper };
    ActiveSlider m_activeSlider = None;

    QRect getLowerThumbRect() const;
    QRect getUpperThumbRect() const;
    QRect getTrackRect() const;

    int posToValue(int x) const;
};

#endif // RANGESLIDER_H
