#include "rangeslider.h"

RangeSlider::RangeSlider(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(30);
    setMouseTracking(true);
}

void RangeSlider::setMinimum(int min) {
    m_minimum = min;
    update();
}

void RangeSlider::setMaximum(int max) {
    m_maximum = max;
    update();
}

void RangeSlider::setLowerValue(int val) {
    if (val < m_minimum) val = m_minimum;
    if (val > m_upperValue) val = m_upperValue;
    if (m_lowerValue != val) {
        m_lowerValue = val;
        emit lowerValueChanged(m_lowerValue);
        update();
    }
}

void RangeSlider::setUpperValue(int val) {
    if (val > m_maximum) val = m_maximum;
    if (val < m_lowerValue) val = m_lowerValue;
    if (m_upperValue != val) {
        m_upperValue = val;
        emit upperValueChanged(m_upperValue);
        update();
    }
}

QRect RangeSlider::getTrackRect() const {
    return QRect(10, height() / 2 - 2, width() - 20, 4);
}

QRect RangeSlider::getLowerThumbRect() const {
    float range = m_maximum - m_minimum;
    float pos = 0;
    if (range > 0) pos = (m_lowerValue - m_minimum) / range;
    
    QRect track = getTrackRect();
    int x = track.left() + pos * track.width();
    return QRect(x - 5, height() / 2 - 8, 10, 16);
}

QRect RangeSlider::getUpperThumbRect() const {
    float range = m_maximum - m_minimum;
    float pos = 0;
    if (range > 0) pos = (m_upperValue - m_minimum) / range;
    
    QRect track = getTrackRect();
    int x = track.left() + pos * track.width();
    return QRect(x - 5, height() / 2 - 8, 10, 16);
}

int RangeSlider::posToValue(int x) const {
    QRect track = getTrackRect();
    if (x < track.left()) return m_minimum;
    if (x > track.right()) return m_maximum;
    
    float ratio = (float)(x - track.left()) / (float)track.width();
    return m_minimum + ratio * (m_maximum - m_minimum);
}

void RangeSlider::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect track = getTrackRect();
    QRect lowerRect = getLowerThumbRect();
    QRect upperRect = getUpperThumbRect();

    // Arkaplan Track (Koyu Gri)
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 60, 60));
    painter.drawRoundedRect(track, 2, 2);

    // Aktif Track (Sarı)
    QRect activeTrack(lowerRect.center().x(), track.top(), upperRect.center().x() - lowerRect.center().x(), track.height());
    painter.setBrush(QColor(255, 180, 0)); // Altın Sarısı
    painter.drawRoundedRect(activeTrack, 2, 2);

    // Thumbs (Tutamaçlar - Açık Gri)
    painter.setBrush(QColor(180, 180, 180));
    painter.setPen(QPen(QColor(40, 40, 40), 1));
    painter.drawRect(lowerRect);
    painter.drawRect(upperRect);
}

void RangeSlider::mousePressEvent(QMouseEvent *event) {
    QRect lowerRect = getLowerThumbRect();
    QRect upperRect = getUpperThumbRect();

    // Tolerans eklendi
    lowerRect.adjust(-5, -5, 5, 5);
    upperRect.adjust(-5, -5, 5, 5);

    if (lowerRect.contains(event->pos())) {
        m_activeSlider = Lower;
    } else if (upperRect.contains(event->pos())) {
        m_activeSlider = Upper;
    } else {
        m_activeSlider = None;
    }
}

void RangeSlider::mouseMoveEvent(QMouseEvent *event) {
    if (m_activeSlider == None) return;

    int val = posToValue(event->pos().x());
    
    if (m_activeSlider == Lower) {
        if (val > m_upperValue) val = m_upperValue;
        if (m_lowerValue != val) {
            m_lowerValue = val;
            emit lowerPositionChanged(m_lowerValue);
            update();
        }
    } else if (m_activeSlider == Upper) {
        if (val < m_lowerValue) val = m_lowerValue;
        if (m_upperValue != val) {
            m_upperValue = val;
            emit upperPositionChanged(m_upperValue);
            update();
        }
    }
}

void RangeSlider::mouseReleaseEvent(QMouseEvent *event) {
    if (m_activeSlider == Lower) {
        emit lowerValueChanged(m_lowerValue);
    } else if (m_activeSlider == Upper) {
        emit upperValueChanged(m_upperValue);
    }
    m_activeSlider = None;
}
