#pragma once

#ifndef NOTEINK_INCLUDED
#define NOTEINK_INCLUDED

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRect>
#include <QVector>

enum NoteInkToolKind {
  NoteInkPencil0 = 0,
  NoteInkPencil1 = 1,
  NoteInkPencil2 = 2,
  NoteInkEraser  = 3
};

const int NoteInkPencilCount = 3;
const int NoteInkSliderInset = 4;
const int NoteInkChipMargin  = 1;

inline QRect noteInkIconSlotRect(int slotIndex, int nSlots, int slotGap,
                                 const QRect &box, int iconRowH) {
  int slotW = (box.width() - slotGap * (nSlots - 1)) / nSlots;
  if (slotW < 4) slotW = 4;
  int x = box.left() + slotIndex * (slotW + slotGap);
  return QRect(x, box.top(), slotW, iconRowH);
}

inline QRect noteInkSlotChipSquare(const QRect &slot) {
  QRect inner = slot.adjusted(1, 1, -1, -1);
  int s       = qMin(inner.width(), inner.height());
  if (s < 2) return inner;
  return QRect(inner.x() + (inner.width() - s) / 2,
               inner.y() + (inner.height() - s) / 2, s, s);
}

struct NoteInkStroke {
  QVector<QPointF> points;
  QColor color  = QColor(40, 40, 40);
  double width = 1.6;
};

typedef QList<NoteInkStroke> NoteInkStrokeList;

inline QColor defaultNoteInkPencil(int index) {
  switch (index) {
  case 1:
    return QColor(180, 40, 40);
  case 2:
    return QColor(30, 70, 170);
  default:
    return QColor(40, 40, 40);
  }
}

#endif  // NOTEINK_INCLUDED
