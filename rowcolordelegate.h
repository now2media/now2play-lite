#ifndef ROWCOLORDELEGATE_H
#define ROWCOLORDELEGATE_H

#include "globalproperties.h"
#include <QPainter>
#include <QStyledItemDelegate>

class RowColorDelegate : public QStyledItemDelegate {
public:
  explicit RowColorDelegate(globalProperties *props, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), m_globalProps(props) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    int row = index.row();

    // Renkleri tanımlayalım (Kullanıcının VB kodundaki renkler)
    QColor playedColor(133, 133, 134); // Status 0: Gri
    QColor playingColor(154, 205, 50); // Status 1: YellowGreen

    bool hasStatus = false;
    QColor bgColor;

    QString pathOrCmd = index.sibling(row, 0).data(Qt::UserRole).toString();
    bool isCmd = pathOrCmd.startsWith("CMD:");

    if (isCmd) {
      bgColor = QColor("#5a2929"); // Komut satırları için mat kırmızı ton
      hasStatus = true;
    } else if (m_globalProps && m_globalProps->selectfile != -1) {
      if (row < m_globalProps->selectfile) {
        bgColor = playedColor;
        hasStatus = true;
      } else if (row == m_globalProps->selectfile) {
        bgColor = playingColor;
        hasStatus = true;
      }
    }

    if (hasStatus) {
      if (option.state & QStyle::State_Selected) {
        // Seçili olduğunda koyu bir ton veya seçim rengi ile karıştırılabilir
        // Şimdilik sadece seçim rengini gösterelim veya üzerine bir katman
        // atalım
        painter->fillRect(option.rect, option.palette.highlight());
      } else {
        painter->fillRect(option.rect, bgColor);
      }
    }

    // Metin rengini ayarla (Okunabilirlik için)
    if (hasStatus && !(option.state & QStyle::State_Selected)) {
      if (row < m_globalProps->selectfile)
        opt.palette.setColor(QPalette::Text, Qt::white);
      else if (row == m_globalProps->selectfile)
        opt.palette.setColor(QPalette::Text, Qt::black);
    }

    QStyledItemDelegate::paint(painter, opt, index);

    // Sürükle-bırak sırasında hedef satırın üstüne veya altına vurgu çizgisini
    // ekle
    if (m_globalProps) {
      if (m_globalProps->dropIndicatorRow == row) {
        painter->save();
        QPen pen(
            QColor(85, 170, 255)); // ProgressBar rengiyle uyumlu (Mavi tonları)
        pen.setWidth(4);
        painter->setPen(pen);
        painter->drawLine(option.rect.topLeft(), option.rect.topRight());
        painter->restore();
      } else if (index.model() && row == index.model()->rowCount() - 1 &&
                 m_globalProps->dropIndicatorRow == index.model()->rowCount()) {
        painter->save();
        QPen pen(QColor(85, 170, 255));
        pen.setWidth(4);
        painter->setPen(pen);
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        painter->restore();
      }
    }
  }

private:
  globalProperties *m_globalProps;
};

#endif // ROWCOLORDELEGATE_H
