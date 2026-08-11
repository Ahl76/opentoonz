

#include "dvitemview.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tlevel_io.h"
#include "tfiletype.h"
#include "tsystem.h"

// Qt includes
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QAction>
#include <QDate>
#include <QContextMenuEvent>
#include <QMenu>
#include <QScrollBar>
#include <QFileInfo>
#include <QFileDialog>
#include <QTextStream>
#include <qdrawutil.h>
#include <QMimeData>
#include <QSlider>
#include <QToolButton>
#include <QTimer>
#include <QLineEdit>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QApplication>
#include <cmath>
#include <QGuiApplication>
#include <QCoreApplication>

#include "toonzqt/colorfield.h"
#include "pane.h"

#include <stdint.h>  // for uint64_t

namespace {
const int kBrowserIconQuantStep    = 16;
// Debounce HD regen while scrubbing the size control.
const int kBrowserRenderDebounceMs = 280;
const int kBrowserThumbMinWidth    = 40;
const int kBrowserThumbMaxWidth    = 400;
const int kBrowserThumbDefaultW    = 80;
const int kBrowserThumbDefaultH    = 60;
const int kBrowserBlockSepPad = 6;
const int kBrowserIconGap = 4;

QColor browserBlockSepColor(const QWidget *widget) {
  if (!widget) return QColor(0x4f, 0x4f, 0x4f);
  const QPalette pal = widget->palette();
  const QColor bgWin = pal.color(QPalette::Window);
  const bool lightChrome = bgWin.lightness() > 120;
  if (lightChrome) return pal.color(QPalette::Text).darker(118);
  const QColor mid = pal.color(QPalette::Mid);
  return mid.isValid() ? mid.darker(130) : QColor(0x4f, 0x4f, 0x4f);
}

class BrowserBlockSepLine final : public QWidget {
public:
  explicit BrowserBlockSepLine(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(1, 18);
    setAttribute(Qt::WA_OpaquePaintEvent);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    const QWidget *toolbar = parentWidget();
    if (toolbar) toolbar = toolbar->parentWidget();
    QPainter p(this);
    p.fillRect(rect(), browserBlockSepColor(toolbar));
  }

  void changeEvent(QEvent *event) override {
    QWidget::changeEvent(event);
    const QEvent::Type type = event->type();
    if (type == QEvent::PaletteChange || type == QEvent::StyleChange) update();
  }
};

//! Project-folder shortcut button (folder icon + letter label).
class ProjectFolderShortcutButton final : public QToolButton {
  QString m_label;

public:
  ProjectFolderShortcutButton(const QString &label, QWidget *parent = nullptr)
      : QToolButton(parent), m_label(label) {
    setIcon(createQIcon("folder"));
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    QToolButton::paintEvent(event);
    if (m_label.isEmpty()) return;

    const QRect cr = contentsRect();
    const QSize is = iconSize();
    const QRect iconRect(cr.x() + (cr.width() - is.width()) / 2,
                         cr.y() + (cr.height() - is.height()) / 2, is.width(),
                         is.height());

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont font = p.font();
    font.setPixelSize(m_label.size() > 1 ? 8 : 10);
    font.setBold(true);
    p.setFont(font);
    p.setPen(QColor(25, 25, 25));

    const QFontMetrics fm(font);
    const QRect tight = fm.tightBoundingRect(m_label);
    const int cx = iconRect.center().x();
    const int cy = iconRect.top() + qRound(iconRect.height() * 0.55);
    QRect target(0, 0, qMax(tight.width() + 2, 8), qMax(tight.height() + 2, 8));
    target.moveCenter(QPoint(cx, cy));
    p.drawText(target, Qt::AlignCenter, m_label);
  }
};

// Use Inactive colors so focus / dialog open does not shift the placeholder.
static QColor browserSearchPlaceholderColor(const QPalette &pal) {
  QColor hint = pal.color(QPalette::Inactive, QPalette::PlaceholderText);
  if (hint.isValid() && hint.alpha() > 0) return hint;

  hint = pal.color(QPalette::Inactive, QPalette::Mid);
  if (hint.isValid() && hint.alpha() > 0) return hint;

  const QColor text = pal.color(QPalette::Inactive, QPalette::Text);
  const QColor base = pal.color(QPalette::Inactive, QPalette::Base);
  if (text.isValid() && base.isValid()) {
    const int wt = 11, wb = 14, sum = wt + wb;
    return QColor((text.red() * wt + base.red() * wb) / sum,
                  (text.green() * wt + base.green() * wb) / sum,
                  (text.blue() * wt + base.blue() * wb) / sum);
  }

  return pal.color(QPalette::Inactive, QPalette::WindowText);
}

void applyBrowserSearchPlaceholderStyle(QLineEdit *edit) {
  if (!edit) return;
  static bool inApply = false;
  if (inApply) return;
  inApply = true;

  const QColor hint = browserSearchPlaceholderColor(edit->palette());
  if (hint.isValid()) {
    const QString rule = edit->objectName().isEmpty()
                             ? QStringLiteral("QLineEdit::placeholder")
                             : QStringLiteral("#%1::placeholder")
                                   .arg(edit->objectName());
    const QString sheet = QStringLiteral("%1 { color: %2; }")
                              .arg(rule, hint.name(QColor::HexRgb));
    if (edit->styleSheet() != sheet) edit->setStyleSheet(sheet);
  }

  inApply = false;
}

}  // namespace

// GUI Show/Hide parts (Viewer-style bit flags).
enum BrowserAdvancedGuiPart {
  AGUI_SizeSlider      = 0x01,
  AGUI_SizeMenu        = 0x02,
  AGUI_Background      = 0x04,
  AGUI_TypeFilters     = 0x08,
  AGUI_Search          = 0x10,
  AGUI_PlayFps         = 0x20,
  AGUI_TypeFilterList  = 0x40,
  AGUI_Favorites       = 0x80,
  AGUI_FavoriteStars   = 0x100,
  AGUI_ProjectFolders  = 0x200,
  AGUI_InfoPanel       = 0x400,
  AGUI_All = AGUI_SizeSlider | AGUI_SizeMenu | AGUI_Background | AGUI_TypeFilters |
             AGUI_Search | AGUI_PlayFps | AGUI_TypeFilterList | AGUI_Favorites |
             AGUI_FavoriteStars | AGUI_ProjectFolders | AGUI_InfoPanel,
};

TEnv::IntVar BrowserView("BrowserView", 1);
TEnv::IntVar CastView("CastView", 1);
TEnv::IntVar BrowserAdvancedDisplay("BrowserAdvancedDisplay", 0);
TEnv::IntVar CastAdvancedDisplay("CastAdvancedDisplay", 0);
TEnv::IntVar BrowserAdvancedGuiParts("BrowserAdvancedGuiParts", (int)AGUI_All);
TEnv::IntVar CastAdvancedGuiParts("CastAdvancedGuiParts", (int)AGUI_All);
TEnv::IntVar BrowserPlayFps("BrowserPlayFps", 10);  // legacy interval = 100 ms
TEnv::IntVar CastPlayFps("CastPlayFps", 10);
TEnv::IntVar BrowserPlayLoop("BrowserPlayLoop", 1);
TEnv::IntVar CastPlayLoop("CastPlayLoop", 1);
TEnv::IntVar BrowserThumbnailBg("BrowserThumbnailBg", 4);
TEnv::IntVar CastThumbnailBg("CastThumbnailBg", 4);
TEnv::IntVar BrowserThumbnailWidth("BrowserThumbnailWidth", 80);
TEnv::IntVar CastThumbnailWidth("CastThumbnailWidth", 80);
TEnv::IntVar BrowserFileSizeisVisible("BrowserFileSizeisVisible", 1);
TEnv::IntVar BrowserFrameCountisVisible("BrowserFrameCountisVisible", 1);
TEnv::IntVar BrowserCreationDateisVisible("BrowserCreationDateisVisible", 1);
TEnv::IntVar BrowserModifiedDateisVisible("BrowserModifiedDateisVisible", 1);
TEnv::IntVar BrowserFileTypeisVisible("BrowserFileTypeisVisible", 1);
TEnv::IntVar BrowserVersionControlStatusisVisible(
    "BrowserVersionControlStatusisVisible", 1);

namespace {

bool browserFavoriteStarsVisible() {
  return (BrowserAdvancedGuiParts & AGUI_FavoriteStars) != 0;
}

void drawFavoriteStar(QPainter &p, const QRect &rect) {
  const qreal cx = rect.center().x();
  const qreal cy = rect.center().y();
  const qreal r  = qMin(rect.width(), rect.height()) * 0.5;
  QPolygonF star;
  star.reserve(10);
  for (int i = 0; i < 10; ++i) {
    const qreal angle  = M_PI * i / 5.0 - M_PI_2;
    const qreal radius = (i % 2 == 0) ? r : r * 0.42;
    star << QPointF(cx + radius * std::cos(angle), cy + radius * std::sin(angle));
  }
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 190, 0));
  p.drawPolygon(star);
}

}  // namespace

//************************************************************************
//    Local namespace  stuff
//************************************************************************

namespace {

void getFileFids(TFilePath path, std::vector<TFrameId> &fids) {
  QFileInfo info(QString::fromStdWString(path.getWideString()));
  TLevelP level;
  if (info.exists()) {
    if (path.getType() == "tnz") {
      try {
        ToonzScene scene;
        scene.loadNoResources(path);
        int i;
        for (i = 0; i < scene.getFrameCount(); i++)
          fids.push_back(TFrameId(i + 1));
      } catch (...) {
      }
    } else if (TFileType::isViewable(TFileType::getInfo(path))) {
      try {
        TLevelReaderP lr(path);
        level = lr->loadInfo();
      } catch (...) {
      }
    }
  } else if (path.isLevelName())  // for levels johndoe..tif etc.
  {
    try {
      TLevelReaderP lr(path);
      level = lr->loadInfo();
    } catch (...) {
    }
  }
  if (level.getPointer()) {
    for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
      fids.push_back(it->first);
  }
}

//-----------------------------------------------------------------------------

QString hyphenText(const QString &srcText, const QFont &font, int width) {
  QFontMetrics metrics(font);
  int srcWidth = metrics.horizontalAdvance(srcText);
  if (srcWidth < width) return srcText;

  int count = double(srcWidth) / double(width);
  int diff  = srcWidth - width * count + 4;  // +4 to keep a margin

  QString text;
  int middleWidth = (double(width) * 0.5);
  int i;
  int hyphenCount = 1;
  for (i = 0; i < srcText.size(); i++) {
    QChar c       = srcText.at(i);
    int cWidth    = metrics.horizontalAdvance(c);
    int textWidth = metrics.horizontalAdvance(text) + cWidth;
    if ((c.isSpace() && textWidth > (hyphenCount - 1) * width + diff) ||
        (textWidth > hyphenCount * width)) {
      ++hyphenCount;
      --i;
      text += "\n";
    } else
      text += c;
  }
  return text;
}

QPixmap getStatusPixmap(int status) {
  static QPixmap bronzePixmap     = QPixmap(":Resources/bronze.png");
  static QPixmap plusPixmap       = QPixmap(":Resources/plus.png");
  static QPixmap greenPixmap      = QPixmap(":Resources/green.png");
  static QPixmap redPixmap        = QPixmap(":Resources/red.png");
  static QPixmap orangePixmap     = QPixmap(":Resources/orange.png");
  static QPixmap grayPixmap       = QPixmap(":Resources/gray.png");
  static QPixmap halfGreenPixmap  = QPixmap(":Resources/halfGreen.png");
  static QPixmap halfBronzePixmap = QPixmap(":Resources/halfBronze.png");
  static QPixmap halfRedPixmap    = QPixmap(":Resources/halfRed.png");

  // Icon
  if (status == DvItemListModel::VC_Locked)
    return bronzePixmap;
  else if (status == DvItemListModel::VC_Edited)
    return greenPixmap;
  else if (status == DvItemListModel::VC_ToUpdate)
    return orangePixmap;
  else if (status == DvItemListModel::VC_Unversioned)
    return plusPixmap;
  else if (status == DvItemListModel::VC_ReadOnly)
    return grayPixmap;
  else if (status == DvItemListModel::VC_PartialEdited)
    return halfGreenPixmap;
  else if (status == DvItemListModel::VC_PartialLocked)
    return halfBronzePixmap;
  else if (status == DvItemListModel::VC_PartialModified)
    return halfRedPixmap;
  else if (status == DvItemListModel::VC_Modified)
    return redPixmap;
  else
    return QPixmap();
}

}  // namespace

//=============================================================================
//
// DvItemListModel
//
//-----------------------------------------------------------------------------

QString DvItemListModel::getItemDataAsString(int index, DataType dataType) {
  QVariant value = getItemData(index, dataType);
  if (value == QVariant()) return "";
  switch (dataType) {
  case Name:
  case ToolTip:
  case FullPath:
    return value.toString();
  case Thumbnail:
  case Icon:
    return "";
  case CreationDate:
    return QLocale::system().toString(value.toDateTime());
    break;
  case ModifiedDate:
    return QLocale::system().toString(value.toDateTime());
    break;
  case FileSize: {
    if (getItemData(index, IsFolder).toBool()) return QString("");

    uint64_t byteSize = value.toLongLong();

    if (byteSize < 1024) return QString::number(byteSize) + " bytes";

    int size = (byteSize) >> 10;  // divide by 1024

    if (size < 1024)
      return QString::number(size) + " KB";
    else if (size < 1024 * 1024)
      return QString::number((double)size / 1024.0) + " MB";
    else
      return QString::number((double)size / (1024 * 1024)) + " GB";
  } break;
  case FrameCount: {
    int frameCount = value.toInt();
    return frameCount > 0 ? QString::number(frameCount) : "";
  } break;
  case VersionControlStatus: {
    Status s = (Status)value.toInt();
    switch (s) {
    case VC_None:
      return QObject::tr("None");
      break;
    case VC_Edited:
      return QObject::tr("Edited");
      break;
    case VC_ReadOnly:
      return QObject::tr("Normal");
      break;
    case VC_ToUpdate:
      return QObject::tr("To Update");
      break;
    case VC_Modified:
      return QObject::tr("Modified");
      break;
    case VC_Locked:
      return QObject::tr("Locked");
      break;
    case VC_Unversioned:
      return QObject::tr("Unversioned");
      break;
    case VC_Missing:
      return QObject::tr("Missing");
      break;
    case VC_PartialEdited:
      return QObject::tr("Partially Edited");
      break;
    case VC_PartialLocked:
      return QObject::tr("Partially Locked");
      break;
    case VC_PartialModified:
      return QObject::tr("Partially Modified");
      break;
    }
    return QObject::tr("None");
  } break;
  case FileType:
    return value.toString();
  default:
    return "";
  }
}

//-----------------------------------------------------------------------------

QString DvItemListModel::getItemDataIdentifierName(DataType dataType) {
  switch (dataType) {
  case Name:
    return QObject::tr("Name");
  case ToolTip:
    return "";
  case FullPath:
    return QObject::tr("Path");
  case Thumbnail:
    return "";
  case Icon:
    return "";
  case CreationDate:
    return QObject::tr("Date Created");
  case ModifiedDate:
    return QObject::tr("Date Modified");
  case FileSize:
    return QObject::tr("Size");
  case FrameCount:
    return QObject::tr("Frames");
  case VersionControlStatus:
    return QObject::tr("Version Control");
  case FileType:
    return QObject::tr("Type");
  default:
    return "";
  }
}

//-----------------------------------------------------------------------------

int DvItemListModel::compareData(DataType dataType, int firstIndex,
                                 int secondIndex) {
  QVariant firstValue  = getItemData(firstIndex, dataType);
  QVariant secondValue = getItemData(secondIndex, dataType);

  switch (dataType) {
  case Name:
  case FileType:
    return QString::localeAwareCompare(firstValue.toString(),
                                       secondValue.toString());

  case CreationDate:
  case ModifiedDate: {
    if (firstValue.toDateTime() < secondValue.toDateTime()) return 1;
    if (firstValue.toDateTime() == secondValue.toDateTime()) return 0;
    if (firstValue.toDateTime() > secondValue.toDateTime()) return -1;
  }

  case FileSize:
    return firstValue.toLongLong() - secondValue.toLongLong();

  case FrameCount:
    return firstValue.toInt() - secondValue.toInt();

  case VersionControlStatus:
    return firstValue.toInt() < secondValue.toInt();

  default:
    break;
  }

  return 0;
}

//=============================================================================
//
// DvItemSelection
//
//-----------------------------------------------------------------------------

DvItemSelection::DvItemSelection() : m_model(0) {}

//-----------------------------------------------------------------------------

void DvItemSelection::select(int index, bool on) {
  if (on)
    m_selectedIndices.insert(index);
  else
    m_selectedIndices.erase(index);
  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::select(int *indices, int indicesCount) {
  m_selectedIndices.clear();
  m_selectedIndices.insert(indices, indices + indicesCount);

  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::selectNone() {
  m_selectedIndices.clear();
  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::selectAll() {
  m_selectedIndices.clear();

  // exclude the parent folder
  int i =
      m_model->getItemData(0, DvItemListModel::Name).toString() == ".." ? 1 : 0;
  for (; i < m_model->getItemCount(); i++) m_selectedIndices.insert(i);
  emit itemSelectionChanged();
}

//-----------------------------------------------------------------------------

void DvItemSelection::setModel(DvItemListModel *model) { m_model = model; }

//-----------------------------------------------------------------------------

void DvItemSelection::enableCommands() {
  if (m_model) m_model->enableSelectionCommands(this);
}

//=============================================================================
//
// ItemViewPlayWidget::PlayManager
//
//-----------------------------------------------------------------------------

ItemViewPlayWidget::PlayManager::PlayManager()
    : m_path(TFilePath())
    , m_currentFidIndex(0)
    , m_pixmap(QPixmap())
    , m_iconSize(QSize())
    , m_renderSize(QSize())
    , m_browserBgMode(0) {}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::PlayManager::reset() {
  int i;
  for (i = 1; i < (int)m_fids.size(); i++)
    IconGenerator::instance()->remove(m_path, m_fids[i]);
  m_fids.clear();
  m_path            = TFilePath();
  m_currentFidIndex = 0;
  m_pixmap          = QPixmap();
  m_browserBgMode   = 0;
}

//-----------------------------------------------------------------------------

QPixmap ItemViewPlayWidget::PlayManager::fetchFramePixmap(const TFrameId &fid,
                                                          bool) {
  QPixmap pixmap;
  const bool wantHd =
      m_renderSize.width() > 0 && m_renderSize.height() > 0 &&
      (m_renderSize.width() > 80 || m_renderSize.height() > 60);
  if (wantHd) {
    const TDimension dim(m_renderSize.width(), m_renderSize.height());
    // Frame 0 / NO_FRAME: same convention as static thumbnails (scene PNG).
    if (fid == TFrameId::NO_FRAME || m_currentFidIndex == 0)
      pixmap = IconGenerator::instance()->getSizedIcon(
          m_path, dim, TFrameId::NO_FRAME, m_browserBgMode);
    else
      pixmap = IconGenerator::instance()->getSizedIcon(m_path, dim, fid,
                                                       m_browserBgMode);
  }
  if (pixmap.isNull()) {
    if (fid == TFrameId::NO_FRAME || m_currentFidIndex == 0)
      pixmap = IconGenerator::instance()->getIcon(m_path);
    else
      pixmap = IconGenerator::instance()->getIcon(m_path, fid);
  }
  return pixmap;
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::PlayManager::setInfo(DvItemListModel *model, int index,
                                              const QSize &layoutSize,
                                              const QSize &renderSize,
                                              int browserBgMode) {
  assert(!!model && index >= 0);
  QString string =
      model->getItemData(index, DvItemListModel::FullPath).toString();
  TFilePath path = TFilePath(string.toStdWString());
  if (!m_path.isEmpty() && !m_fids.empty() &&
      path == m_path)  // Ho gia' il path e i frameId settati correttamente
  {
    m_currentFidIndex = 0;
    m_pixmap          = QPixmap();
    if (!layoutSize.isEmpty()) m_iconSize = layoutSize;
    if (!renderSize.isEmpty()) m_renderSize = renderSize;
    m_browserBgMode = browserBgMode;
    return;
  }

  reset();
  m_iconSize       = layoutSize.isEmpty() ? QSize(80, 60) : layoutSize;
  m_renderSize     = renderSize.isEmpty() ? m_iconSize : renderSize;
  m_browserBgMode  = browserBgMode;
  // Leave empty until the first play frame is ready.
  m_pixmap = QPixmap();
  m_path   = path;
  getFileFids(m_path, m_fids);
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::increaseCurrentFrame() {
  const TFrameId fid =
      (m_currentFidIndex == 0 || m_fids.empty())
          ? TFrameId::NO_FRAME
          : m_fids[m_currentFidIndex];
  QPixmap pixmap = fetchFramePixmap(fid, true);
  if (pixmap.isNull())
    return false;  // Se non ha ancora finito di calcolare l'icona ritorno
  assert(!m_iconSize.isEmpty());
  // Keep native size; paint() scales into the cell.
  m_pixmap = pixmap;
  ++m_currentFidIndex;
  return true;
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::getCurrentFrame() {
  const TFrameId fid =
      (m_currentFidIndex == 0 || m_fids.empty())
          ? TFrameId::NO_FRAME
          : m_fids[m_currentFidIndex];
  QPixmap pixmap = fetchFramePixmap(fid, false);
  if (pixmap.isNull())
    return false;  // Se non ha ancora finito di calcolare l'icona ritorno
  assert(!m_iconSize.isEmpty());
  m_pixmap = pixmap;
  return true;
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::isFrameIndexInRange() {
  return (m_currentFidIndex >= 0 && m_currentFidIndex < m_fids.size());
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::restartFromBeginning() {
  if (m_fids.empty()) return false;
  m_currentFidIndex = 0;
  m_pixmap          = QPixmap();
  getCurrentFrame();  // may still be pending HD — keep the timer running
  return true;
}

//-----------------------------------------------------------------------------

bool ItemViewPlayWidget::PlayManager::setCurrentFrameIndexFromXValue(
    int xValue, int length) {
  if (m_fids.size() == 0) return false;
  double d     = (double)length / (double)(m_fids.size() - 1);
  int newIndex = tround((double)xValue / d);
  if (newIndex == m_currentFidIndex) return false;
  m_currentFidIndex = newIndex;
  assert(isFrameIndexInRange());
  return true;
}

//-----------------------------------------------------------------------------

double ItemViewPlayWidget::PlayManager::currentFrameIndexToXValue(int length) {
  if (m_fids.size() == 0) return false;
  double d = (double)length / (double)(m_fids.size() - 1);
  return d * m_currentFidIndex;
}

//-----------------------------------------------------------------------------

QPixmap ItemViewPlayWidget::PlayManager::getCurrentPixmap() { return m_pixmap; }

//=============================================================================
//
// ItemViewPlayWidget
//
//-----------------------------------------------------------------------------

ItemViewPlayWidget::ItemViewPlayWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentItemIndex(-1)
    , m_timerId(0)
    , m_isSliderMode(false) {
  m_playManager = new PlayManager();
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::play() {
  int fps = 10;  // legacy default → 100 ms (preserves prior behavior)
  if (auto *panel = qobject_cast<DvItemViewerPanel *>(parentWidget()))
    fps = panel->getPlayFps();
  fps         = qBound(1, fps, 120);
  m_timerId   = startTimer(qMax(1, 1000 / fps));
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::refreshPlayInterval() {
  if (m_timerId == 0) return;
  const bool keepIndex = (m_currentItemIndex != -1);
  killTimer(m_timerId);
  m_timerId = 0;
  if (!keepIndex) return;
  int fps = 10;
  if (auto *panel = qobject_cast<DvItemViewerPanel *>(parentWidget()))
    fps = panel->getPlayFps();
  fps       = qBound(1, fps, 120);
  m_timerId = startTimer(qMax(1, 1000 / fps));
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::stop() {
  if (m_timerId != 0) {
    killTimer(m_timerId);
    m_timerId = 0;
  }
  if (!m_isSliderMode) m_currentItemIndex = -1;
}
//-----------------------------------------------------------------------------

void ItemViewPlayWidget::clear() {
  m_isSliderMode = false;
  if (m_currentItemIndex != -1) stop();
  m_playManager->reset();
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::setIsPlaying(DvItemListModel *model, int index) {
  if (isIndexPlaying(index) &&
      !m_isSliderMode)  // Devo fare stop prima di inizializzare un nuovo play
  {
    stop();
    return;
  } else if (m_isSliderMode) {
    m_isSliderMode = false;
  }

  if (m_currentItemIndex == -1) {
    m_currentItemIndex = index;
    QSize layoutSize(80, 60), renderSize(80, 60);
    int bgMode = 0;
    if (auto *panel = qobject_cast<DvItemViewerPanel *>(parentWidget())) {
      layoutSize = panel->getIconSize();
      renderSize = panel->getRenderIconSize();
      bgMode     = (int)panel->getThumbnailBgMode();
    }
    m_playManager->setInfo(model, index, layoutSize, renderSize, bgMode);
    m_playManager->getCurrentFrame();  // prime first HD frame if ready
  }
  play();
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::setIsPlayingCurrentFrameIndex(DvItemListModel *model,
                                                       int index, int xValue,
                                                       int length) {
  m_isSliderMode = true;

  if (m_currentItemIndex == -1) {
    m_currentItemIndex = index;
    QSize layoutSize(80, 60), renderSize(80, 60);
    int bgMode = 0;
    if (auto *panel = qobject_cast<DvItemViewerPanel *>(parentWidget())) {
      layoutSize = panel->getIconSize();
      renderSize = panel->getRenderIconSize();
      bgMode     = (int)panel->getThumbnailBgMode();
    }
    m_playManager->setInfo(model, index, layoutSize, renderSize, bgMode);
  }
  if (!m_playManager->setCurrentFrameIndexFromXValue(xValue, length)) return;
  stop();  // Devo fare stop prima di cambiare il frame corrente
  play();
}

//-----------------------------------------------------------------------------

int ItemViewPlayWidget::getCurrentFramePosition(int length) {
  if (m_playManager->isFrameIndexInRange())
    return m_playManager->currentFrameIndexToXValue(length);
  return 0;
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::paint(QPainter *painter, QRect rect) {
  QPixmap pixmap = m_playManager->getCurrentPixmap();
  if (pixmap.isNull()) return;
  const QRect dest = rect.adjusted(2, 2, -1, -1);
  if (pixmap.size() == dest.size()) {
    painter->drawPixmap(dest.topLeft(), pixmap);
  } else {
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap scaled =
        pixmap.scaled(dest.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QRect centered(scaled.rect());
    centered.moveCenter(dest.center());
    painter->drawPixmap(centered.topLeft(), scaled);
  }
}

//-----------------------------------------------------------------------------

void ItemViewPlayWidget::timerEvent(QTimerEvent *event) {
  if (!m_playManager->isFrameIndexInRange()) {
    // End of sequence: loop or stop (Viewer / Flipbook style).
    bool loop = true;
    if (auto *panel = qobject_cast<DvItemViewerPanel *>(parentWidget()))
      loop = panel->isPlayLoop();
    if (loop && m_playManager->restartFromBeginning()) {
      parentWidget()->update();
      return;
    }
    stop();
    parentWidget()->update();
    return;
  }
  if (m_isSliderMode)  // Modalita' slider
  {
    if (!m_playManager->getCurrentFrame()) return;
    parentWidget()->update();
    stop();
    return;
  }
  // Modalita' play
  if (!m_playManager->increaseCurrentFrame())
    return;
  else
    parentWidget()->update();
}

//=============================================================================
// DVItemViewPlayDelegate
//-----------------------------------------------------------------------------

DVItemViewPlayDelegate::DVItemViewPlayDelegate(QWidget *parent)
    : QObject(parent) {
  m_itemViewPlay = new ItemViewPlayWidget(parent);
}

//-----------------------------------------------------------------------------

bool DVItemViewPlayDelegate::setPlayWidget(DvItemListModel *model, int index,
                                           QRect rect, QPoint pos) {
  bool isPlaying = getPlayButtonRect(rect).contains(pos);
  if (!isPlaying && !getPlaySliderRect(rect).contains(pos)) return false;
  if (isPlaying)
    m_itemViewPlay->setIsPlaying(model, index);
  else
    m_itemViewPlay->setIsPlayingCurrentFrameIndex(
        model, index, pos.x() - getPlaySliderRect(rect).left(),
        getPlaySliderRect(rect).width());
  return true;
}

//-----------------------------------------------------------------------------

void DVItemViewPlayDelegate::resetPlayWidget() { m_itemViewPlay->clear(); }

//-----------------------------------------------------------------------------

void DVItemViewPlayDelegate::paint(QPainter *painter, QRect rect, int index) {
  if (m_itemViewPlay->isIndexPlaying(index) &&
      !m_itemViewPlay->isSliderMode())  // Modalita' play
  {
    QSize iconSize(10, 11);
    static QIcon playIcon = createQIconPNG("iconpause");
    QPixmap pixmap        = playIcon.pixmap(iconSize);
    if (m_itemViewPlay->isIndexPlaying(index))
      m_itemViewPlay->paint(painter, rect);
    painter->drawPixmap(getPlayButtonRect(rect), pixmap);
  } else {
    QSize iconSize(6, 11);
    static QIcon playIcon = createQIconPNG("iconplay");
    QPixmap pixmap        = playIcon.pixmap(iconSize);
    if (m_itemViewPlay->isIndexPlaying(
            index))  // Puo' servire in modalita' slider
      m_itemViewPlay->paint(painter, rect);
    painter->drawPixmap(getPlayButtonRect(rect).adjusted(2, 0, -2, 0), pixmap);
  }
  QRect sliderRect = getPlaySliderRect(rect);
  double xSliderValue =
      sliderRect.left() +
      m_itemViewPlay->getCurrentFramePosition(sliderRect.width()) - 1;
  QRect indicatorRect(xSliderValue, sliderRect.top() + 2, 2, 6);
  sliderRect = sliderRect.adjusted(0, 4, 0, -4);
  QColor sliderColor(171, 206, 255);
  painter->setPen(Qt::black);
  painter->fillRect(sliderRect, QBrush(sliderColor));
  painter->drawRect(sliderRect);
  painter->fillRect(indicatorRect, QBrush(sliderColor));
  painter->drawRect(indicatorRect);
}

//-----------------------------------------------------------------------------

QRect DVItemViewPlayDelegate::getPlayButtonRect(QRect rect) {
  QPoint iconSize = QPoint(10, 11);
  QPoint point    = rect.bottomRight() - iconSize;
  QRect playButtonRect(point.x(), point.y(), iconSize.x(), iconSize.y());
  return playButtonRect;
}

//-----------------------------------------------------------------------------

QRect DVItemViewPlayDelegate::getPlaySliderRect(QRect rect) {
  QPoint point = rect.bottomLeft() + QPoint(5, -10);
  QRect playSliderRect(point.x(), point.y(), rect.width() - 20, 10);
  return playSliderRect;
}

//=============================================================================
//
// DvItemViewerPanel
//
//-----------------------------------------------------------------------------

DvItemViewerPanel::DvItemViewerPanel(DvItemViewer *viewer, bool noContextMenu,
                                     bool multiSelectionEnabled,
                                     QWidget *parent)
    : QFrame(parent)
    , m_selection(0)
    , m_viewer(viewer)
    , m_viewType(viewer->m_windowType == DvItemViewer::Cast
                     ? to_enum(CastView)
                     : to_enum(BrowserView))
    , m_xMargin(0)
    , m_yMargin(0)
    , m_itemSpacing(0)
    , m_itemPerRow(0)
    , m_itemSize(0, 0)
    , m_iconSize(kBrowserThumbDefaultW, kBrowserThumbDefaultH)
    , m_renderIconSize(kBrowserThumbDefaultW, kBrowserThumbDefaultH)
    , m_prevRenderIconSize(kBrowserThumbDefaultW, kBrowserThumbDefaultH)
    , m_currentIndex(-1)
    , m_singleColumnEnabled(false)
    , m_centerAligned(false)
    , m_noContextMenu(noContextMenu)
    , m_isPlayDelegateDisable(true)
    , m_globalSelectionEnabled(true)
    , m_multiSelectionEnabled(multiSelectionEnabled)
    , m_missingColor(Qt::gray)
    , m_advancedDisplay(false)
    , m_thumbnailBgMode(BgAuto)
    , m_playFps(10)
    , m_playLoop(true)
    , m_renderSizeTimer(nullptr) {
  setFrameStyle(QFrame::StyledPanel);
  setFocusPolicy(Qt::StrongFocus);
  // setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
  QSizePolicy sizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
  sizePolicy.setHeightForWidth(true);
  setSizePolicy(sizePolicy);
  m_selection = new DvItemSelection();
  m_selection->setView(this);
  m_selection->setModel(m_viewer->getModel());
  connect(IconGenerator::instance(), SIGNAL(iconGenerated()), this,
          SLOT(update()));

  m_editFld = new DVGui::LineEdit(this);
  m_editFld->hide();
  connect(m_editFld, SIGNAL(editingFinished()), this, SLOT(rename()));

  m_columns.push_back(
      std::make_pair(DvItemListModel::Name, std::make_pair(200, 1)));

  m_renderSizeTimer = new QTimer(this);
  m_renderSizeTimer->setSingleShot(true);
  connect(m_renderSizeTimer, SIGNAL(timeout()), this,
          SLOT(commitRenderIconSize()));

  // Restore advanced display settings for this window type.
  const bool isCast = viewer->m_windowType == DvItemViewer::Cast;
  m_advancedDisplay =
      isCast ? (bool)CastAdvancedDisplay : (bool)BrowserAdvancedDisplay;
  m_playFps = isCast ? (int)CastPlayFps : (int)BrowserPlayFps;
  if (m_playFps < 1) m_playFps = 10;
  m_playLoop =
      isCast ? ((int)CastPlayLoop != 0) : ((int)BrowserPlayLoop != 0);
  const int bg =
      isCast ? (int)CastThumbnailBg : (int)BrowserThumbnailBg;
  m_thumbnailBgMode = (ThumbnailBgMode)std::max(
      0, std::min(bg, (int)BgAuto));
  const int width =
      isCast ? (int)CastThumbnailWidth : (int)BrowserThumbnailWidth;
  if (m_advancedDisplay) {
    m_iconSize             = sizeFromWidth(width);
    m_renderIconSize       = quantizeRenderSize(m_iconSize);
    m_prevRenderIconSize   = m_renderIconSize;
  }
}

//-----------------------------------------------------------------------------

QSize DvItemViewerPanel::sizeFromWidth(int width) {
  width = std::max(kBrowserThumbMinWidth, std::min(width, kBrowserThumbMaxWidth));
  const int height =
      std::max(1, width * kBrowserThumbDefaultH / kBrowserThumbDefaultW);
  return QSize(width, height);
}

//-----------------------------------------------------------------------------

QSize DvItemViewerPanel::quantizeRenderSize(const QSize &layout) {
  if (layout.width() <= 0 || layout.height() <= 0)
    return QSize(kBrowserThumbDefaultW, kBrowserThumbDefaultH);
  auto quantize = [](int v) {
    if (v <= 0) return 1;
    const int q =
        ((v + kBrowserIconQuantStep / 2) / kBrowserIconQuantStep) *
        kBrowserIconQuantStep;
    return std::max(kBrowserIconQuantStep, q);
  };
  const int w = quantize(layout.width());
  const int h =
      std::max(1, w * kBrowserThumbDefaultH / kBrowserThumbDefaultW);
  return QSize(w, h);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setIconSize(QSize size) {
  if (size.width() <= 0 || size.height() <= 0) return;
  if (m_iconSize == size) return;

  // Anchor zoom on the viewport-center item.
  int anchorIndex = -1;
  if (m_viewer && m_viewType == ThumbnailView) {
    const QPoint viewCenter = m_viewer->viewport()->rect().center();
    const QPoint contentPos =
        mapFrom(m_viewer->viewport(), viewCenter);
    anchorIndex = pos2index(contentPos);
  }

  m_iconSize = size;
  if (m_viewer) m_viewer->updateContentSize();

  if (m_viewer && anchorIndex >= 0 && anchorIndex < getItemCount()) {
    const QRect itemRect = index2pos(anchorIndex);
    const QPoint target  = itemRect.center();
    const int viewH      = m_viewer->viewport()->height();
    const int viewW      = m_viewer->viewport()->width();
    if (QScrollBar *vsb = m_viewer->verticalScrollBar())
      vsb->setValue(qBound(0, target.y() - viewH / 2, vsb->maximum()));
    if (QScrollBar *hsb = m_viewer->horizontalScrollBar()) {
      if (hsb->maximum() > 0)
        hsb->setValue(qBound(0, target.x() - viewW / 2, hsb->maximum()));
    }
  }

  scheduleRenderSizeCommit();
  emit thumbnailSizeChanged(m_iconSize);
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setThumbnailWidth(int width) {
  setIconSize(sizeFromWidth(width));
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setThumbnailBgMode(ThumbnailBgMode mode) {
  if (m_thumbnailBgMode == mode) return;
  m_thumbnailBgMode = mode;
  // Invalidate sized icons when the letterbox / fill mode changes.
  IconGenerator::instance()->clearRequests();
  IconGenerator::instance()->purgeResponsiveFileIconsExcept(TDimension());
  if (m_itemViewPlayDelegate) m_itemViewPlayDelegate->resetPlayWidget();
  emit thumbnailBgModeChanged((int)mode);
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setAdvancedDisplay(bool on) {
  if (m_advancedDisplay == on) return;
  m_advancedDisplay = on;
  if (!on) {
    m_iconSize           = QSize(kBrowserThumbDefaultW, kBrowserThumbDefaultH);
    m_renderIconSize     = m_iconSize;
    m_prevRenderIconSize = m_iconSize;
    if (m_renderSizeTimer) m_renderSizeTimer->stop();
    m_thumbnailBgMode = BgAuto;
    IconGenerator::instance()->clearRequests();
    IconGenerator::instance()->purgeResponsiveFileIconsExcept(TDimension());
  } else {
    const bool isCast = m_viewer && m_viewer->m_windowType == DvItemViewer::Cast;
    const int width =
        isCast ? (int)CastThumbnailWidth : (int)BrowserThumbnailWidth;
    m_iconSize = sizeFromWidth(width > 0 ? width : kBrowserThumbDefaultW);
    commitRenderIconSize();
    const int bg =
        isCast ? (int)CastThumbnailBg : (int)BrowserThumbnailBg;
    m_thumbnailBgMode = (ThumbnailBgMode)std::max(
        0, std::min(bg, (int)BgAuto));
  }
  if (m_viewer) m_viewer->updateContentSize();
  emit advancedDisplayChanged(on);
  emit thumbnailSizeChanged(m_iconSize);
  emit thumbnailBgModeChanged((int)m_thumbnailBgMode);
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setPlayFps(int fps) {
  fps = qBound(1, fps, 120);
  if (m_playFps == fps) return;
  m_playFps = fps;
  const bool isCast = m_viewer && m_viewer->m_windowType == DvItemViewer::Cast;
  if (isCast)
    CastPlayFps = fps;
  else
    BrowserPlayFps = fps;
  emit playFpsChanged(fps);
  if (m_itemViewPlayDelegate) m_itemViewPlayDelegate->refreshPlayInterval();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setPlayLoop(bool loop) {
  if (m_playLoop == loop) return;
  m_playLoop            = loop;
  const bool isCast = m_viewer && m_viewer->m_windowType == DvItemViewer::Cast;
  if (isCast)
    CastPlayLoop = loop ? 1 : 0;
  else
    BrowserPlayLoop = loop ? 1 : 0;
  emit playLoopChanged(loop);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::applyThumbnailSizePreset(ThumbnailSizePreset preset) {
  switch (preset) {
  case SizeList:
    setTableView();
    return;
  case SizeSmall:
    setThumbnailWidth(64);
    break;
  case SizeMedium:
    setThumbnailWidth(80);
    break;
  case SizeLarge:
    setThumbnailWidth(128);
    break;
  case SizeExtraLarge:
    setThumbnailWidth(192);
    break;
  case SizeHuge:
    setThumbnailWidth(320);
    break;
  }
  if (m_viewType != ThumbnailView) setThumbnailsView();
  // Presets jump to a fixed size — commit HD immediately (no debounce wait).
  commitRenderIconSize();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::scheduleRenderSizeCommit() {
  if (!m_renderSizeTimer) return;
  m_renderSizeTimer->start(kBrowserRenderDebounceMs);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::commitRenderIconSize() {
  const QSize next =
      m_advancedDisplay ? quantizeRenderSize(m_iconSize)
                        : QSize(kBrowserThumbDefaultW, kBrowserThumbDefaultH);
  if (next == m_renderIconSize) {
    update();
    return;
  }
  const QSize prev     = m_renderIconSize;
  m_prevRenderIconSize = prev;
  m_renderIconSize     = next;

  // Cells pick up the new size via getItemData. Cast rebuilds its items.
  if (m_viewer && m_viewer->m_windowType == DvItemViewer::Cast) {
    if (DvItemListModel *model = m_viewer->getModel()) model->refreshData();
    m_viewer->refresh();
  } else {
    update();
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::fillThumbnailBackground(QPainter &p,
                                                const QRect &iconRect,
                                                ThumbnailBgMode mode) const {
  if (!m_advancedDisplay) return;
  switch (mode) {
  case BgWhite:
    p.fillRect(iconRect, Qt::white);
    break;
  case BgBlack:
    p.fillRect(iconRect, Qt::black);
    break;
  case BgCheckered:
    p.fillRect(iconRect,
               QBrush(DVGui::CommonChessboard::instance()->getPixmap()));
    break;
  case BgTransparent:
  case BgAuto:
  default:
    // No UI fill; draw the thumbnail as-is.
    break;
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setItemViewPlayDelegate(
    DVItemViewPlayDelegate *playDelegate) {
  m_isPlayDelegateDisable = false;
  m_itemViewPlayDelegate  = playDelegate;
}

//-----------------------------------------------------------------------------

DVItemViewPlayDelegate *DvItemViewerPanel::getItemViewPlayDelegate() {
  if (m_isPlayDelegateDisable) return 0;
  return m_itemViewPlayDelegate;
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::addColumn(DvItemListModel::DataType dataType,
                                  int width) {
  bool val;
  switch (dataType) {
  case DvItemListModel::FileSize:
    val = (bool)BrowserFileSizeisVisible;
    break;
  case DvItemListModel::FrameCount:
    val = (bool)BrowserFrameCountisVisible;
    break;
  case DvItemListModel::CreationDate:
    val = (bool)BrowserCreationDateisVisible;
    break;
  case DvItemListModel::ModifiedDate:
    val = (bool)BrowserModifiedDateisVisible;
    break;
  case DvItemListModel::FileType:
    val = (bool)BrowserFileTypeisVisible;
    break;
  case DvItemListModel::VersionControlStatus:
    val = (bool)BrowserVersionControlStatusisVisible;
    break;
  default:
    val = true;
  }
  m_columns.push_back(std::make_pair(dataType, std::make_pair(width, val)));
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setColumnWidth(DvItemListModel::DataType dataType,
                                       int width) {
  int i;
  for (i = 0; i < m_columns.size(); i++) {
    if (m_columns[i].first != dataType)
      continue;
    else
      m_columns[i].second.first = width;
  }
  update();
}

//-----------------------------------------------------------------------------

bool DvItemViewerPanel::getVisibility(DvItemListModel::DataType dataType) {
  int i;
  for (i = 0; i < m_columns.size(); i++) {
    if (m_columns[i].first != dataType)
      continue;
    else
      return m_columns[i].second.second;
  }
  return 0;
}
//-----------------------------------------------------------------------------

void DvItemViewerPanel::setVisibility(DvItemListModel::DataType dataType,
                                      bool value) {
  int i;
  for (i = 0; i < m_columns.size(); i++) {
    if (m_columns[i].first != dataType)
      continue;
    else
      m_columns[i].second.second = value;
  }
  switch (dataType) {
  case DvItemListModel::FileSize:
    BrowserFileSizeisVisible = value;
    break;
  case DvItemListModel::FrameCount:
    BrowserFrameCountisVisible = value;
    break;
  case DvItemListModel::CreationDate:
    BrowserCreationDateisVisible = value;
    break;
  case DvItemListModel::ModifiedDate:
    BrowserModifiedDateisVisible = value;
    break;
  case DvItemListModel::FileType:
    BrowserFileTypeisVisible = value;
    break;
  case DvItemListModel::VersionControlStatus:
    BrowserVersionControlStatusisVisible = value;
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------

DvItemListModel *DvItemViewerPanel::getModel() const {
  return m_viewer->getModel();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setSelection(DvItemSelection *selection) {
  if (m_selection == selection) return;
  delete m_selection;
  m_selection = selection;
  m_selection->setModel(getModel());
}

//-----------------------------------------------------------------------------

const std::set<int> &DvItemViewerPanel::getSelectedIndices() const {
  return m_selection->getSelectedIndices();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::updateViewParameters(int panelWidth) {
  m_itemPerRow  = 1;
  m_xMargin     = 5;
  m_yMargin     = 5;
  m_itemSpacing = 5;
  int w;

  switch (m_viewType) {
  case ListView:
    m_itemSize    = QSize(panelWidth, fontMetrics().height());
    m_itemSpacing = 0;
    break;
  case TableView: {
    m_itemSize    = QSize(panelWidth, fontMetrics().height() + 7);
    m_itemSpacing = 0;
    m_xMargin     = 0;
    m_yMargin     = 0;
    break;
  }
  case ThumbnailView:
    m_itemSize = QSize(m_iconSize.width() + 10, m_iconSize.height() + 30);
    if (!m_singleColumnEnabled) {
      int w        = panelWidth - m_xMargin * 2 + m_itemSpacing;
      int iw       = m_itemSize.width() + m_itemSpacing;
      m_itemPerRow = w / iw;
      if (m_itemPerRow < 1) m_itemPerRow = 1;
    }
    w = (panelWidth + m_itemSpacing -
         m_itemPerRow * (m_itemSize.width() + m_itemSpacing)) /
        2;
    if (w > m_xMargin) m_xMargin = w;
    break;
  }
  if (m_centerAligned) {
    int rowCount = (getItemCount() + m_itemPerRow - 1) / m_itemPerRow;
    int contentHeight =
        rowCount * (m_itemSize.height() + m_itemSpacing) - m_itemSpacing;
    int parentHeight = parentWidget()->height();
    if (contentHeight + 2 * m_yMargin < parentHeight)
      m_yMargin = (parentHeight - contentHeight) / 2;
  }
}

//-----------------------------------------------------------------------------

int DvItemViewerPanel::pos2index(const QPoint &pos) const {
  int xDist = (pos.x() - m_xMargin);
  int col   = (xDist < 0) ? -1 : xDist / (m_itemSize.width() + m_itemSpacing);
  int yDist = (pos.y() - m_yMargin);
  int row   = (yDist < 0) ? -1 : yDist / (m_itemSize.height() + m_itemSpacing);
  return row * m_itemPerRow + col;
}

//-----------------------------------------------------------------------------

QRect DvItemViewerPanel::index2pos(int index) const {
  int row = index / m_itemPerRow;
  int col = index - row * m_itemPerRow;
  QPoint pos(m_xMargin + (m_itemSize.width() + m_itemSpacing) * col,
             m_yMargin + (m_itemSize.height() + m_itemSpacing) * row);
  return QRect(pos, m_itemSize);
}

//-----------------------------------------------------------------------------

QRect DvItemViewerPanel::getCaptionRect(int index) const {
  QRect itemRect = index2pos(index);
  //  TDimension m_iconSize(80,60);// =
  //  IconGenerator::instance()->getIconSize();
  int y = itemRect.top() + m_iconSize.height();
  QRect textRect(itemRect.left(), y, itemRect.width(), itemRect.bottom() - y);

  return textRect;
}

//-----------------------------------------------------------------------------

int DvItemViewerPanel::getContentMinimumWidth() {
  switch (m_viewType) {
  case ListView:
    return 200;
    break;
  case TableView:
    return 600;
    break;
  case ThumbnailView:
    return 120;
    break;
  default:
    return 120;
    break;
  }
}

//-----------------------------------------------------------------------------

int DvItemViewerPanel::getContentHeight(int width) {
  updateViewParameters(width);
  int itemCount = getItemCount();
  QRect rect    = index2pos(itemCount - 1);
  return rect.bottom() + m_yMargin;
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::paintEvent(QPaintEvent *) {
  QPainter p(this);
  int i, n = getItemCount();
  updateViewParameters(width());
  switch (m_viewType) {
  case ListView:
    for (i = 0; i < n; i++) paintListItem(p, i);
    break;
  case TableView:
    for (i = 0; i < n; i++) paintTableItem(p, i);
    break;
  case ThumbnailView:
    for (i = 0; i < n; i++) paintThumbnailItem(p, i);
    break;
  }

  /*

p.setPen(Qt::green);
for(i=0;i<n;i++)
p.drawRect(index2pos(i));

int y = getContentHeight(width());
p.drawLine(0,y,width(),y);

p.setPen(Qt::magenta);
p.drawRect(0,0,width()-1,height()-1);
*/

  m_viewer->draw(p);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setMissingTextColor(const QColor &color) {
  m_missingColor = color;
}

//-----------------------------------------------

void DvItemViewerPanel::paintThumbnailItem(QPainter &p, int index) {
  // Get Version Control Status
  int status = getModel()
                   ->getItemData(index, DvItemListModel::VersionControlStatus)
                   .toInt();

  bool isSelected = m_selection->isSelected(index);
  QRect rect      = index2pos(index);
  if (!visibleRegion().intersects(rect)) return;
  if (!getModel()) return;

  QRect iconRect(rect.left() + (rect.width() - m_iconSize.width()) / 2,
                 rect.top(), m_iconSize.width(), m_iconSize.height());
  QRect textRect(iconRect.left(), iconRect.bottom(), iconRect.width(),
                 rect.bottom() - iconRect.bottom());

  // Draw Selection
  if (isSelected) {
    p.setPen(Qt::NoPen);
    p.fillRect(iconRect.adjusted(-2, -2, 2, 2), getSelectedItemBackground());
    p.fillRect(textRect.adjusted(-2, 3, 2, 0), getSelectedItemBackground());
  }

  // Thumbnail background fill (folders keep the panel default).
  const bool isFolder =
      getModel()->getItemData(index, DvItemListModel::IsFolder).toBool();
  ThumbnailBgMode thumbBg = m_thumbnailBgMode;
  if (!isFolder) {
    const QVariant bg =
        getModel()->getItemData(index, DvItemListModel::ThumbnailBg);
    if (bg.isValid()) thumbBg = (ThumbnailBgMode)bg.toInt();
  }
  if (!isFolder) fillThumbnailBackground(p, iconRect, thumbBg);

  // Skip the static thumb while play frames are shown.
  const bool playingHere =
      !m_isPlayDelegateDisable && m_itemViewPlayDelegate &&
      m_itemViewPlayDelegate->isIndexPlaying(index);

  // Draw Pixmap
  // if(status != DvItemListModel::VC_Missing)
  //{

  if (!playingHere) {
    QPixmap thumbnail =
        getModel()
            ->getItemData(index, DvItemListModel::Thumbnail, isSelected)
            .value<QPixmap>();
    if (!thumbnail.isNull()) {
      // svgToPixmap / HiDPI entries store physical pixels with a device ratio.
      const qreal dpr =
          thumbnail.devicePixelRatio() > 0 ? thumbnail.devicePixelRatio() : 1.0;
      const QSize logicalSize(qRound(thumbnail.width() / dpr),
                              qRound(thumbnail.height() / dpr));
      if (logicalSize == iconRect.size()) {
        p.drawPixmap(iconRect.topLeft(), thumbnail);
      } else {
        // Scale cached thumb into the live cell while HD regenerates.
        const QPixmap scaled = thumbnail.scaled(
            iconRect.size() * dpr, Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        QPixmap drawPm = scaled;
        drawPm.setDevicePixelRatio(dpr);
        QRect dest(QPoint(0, 0),
                   QSize(qRound(drawPm.width() / dpr),
                         qRound(drawPm.height() / dpr)));
        dest.moveCenter(iconRect.center());
        p.drawPixmap(dest.topLeft(), drawPm);
      }
    } else {
      static QPixmap missingPixmap(getIconPath("missing_icon"));
      QRect pixmapRect(rect.left() + (rect.width() - missingPixmap.width()) / 2,
                       rect.top(), missingPixmap.width(),
                       missingPixmap.height());
      p.drawPixmap(pixmapRect.topLeft(), missingPixmap);
    }
  }

  if (m_advancedDisplay && !isFolder && browserFavoriteStarsVisible()) {
    const bool isFav =
        getModel()->getItemData(index, DvItemListModel::IsFavorite).toBool();
    if (isFav) {
      const QRect starRect(iconRect.right() - 13, iconRect.top() + 1, 12, 12);
      drawFavoriteStar(p, starRect);
    }
  }

  // Draw Text
  if (status == DvItemListModel::VC_Missing)
    p.setPen(m_missingColor);
  else
    p.setPen((isSelected) ? getSelectedTextColor() : getTextColor());

  QString name =
      getModel()->getItemData(index, DvItemListModel::Name).toString();
  int frameCount =
      getModel()->getItemData(index, DvItemListModel::FrameCount).toInt();
  if (frameCount > 0) {
    QString num;
    name += QString(" [") + num.number(frameCount) + QString("]");
  }
  QString elideName = elideText(name, p.font(), 2 * textRect.width());
  p.drawText(textRect, Qt::AlignCenter,
             hyphenText(elideName, p.font(), textRect.width()));

  if (isSelected) {
    if (!m_isPlayDelegateDisable &&
        getModel()->getItemData(index, DvItemListModel::PlayAvailable).toBool())
      m_itemViewPlayDelegate->paint(&p, iconRect.adjusted(-2, -2, 1, 1), index);
  }

  // Draw Scene rect
  if (getModel()->isSceneItem(index)) {
    QRect r(iconRect.left(), iconRect.top(), iconRect.width(), 4);
    p.setPen(Qt::black);
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.fillRect(r.adjusted(1, 1, -1, -1), Qt::white);
    p.setPen(isSelected ? Qt::blue : Qt::black);
    int x;
    for (x = r.left() + 5; x + 5 < r.right(); x += 10) {
      int y = r.top() + 1;
      p.drawLine(x, y, x + 4, y);
      y++;
      x++;
      p.drawLine(x, y, x + 4, y);
    }
  }

  if (status != DvItemListModel::VC_None &&
      status != DvItemListModel::VC_Missing) {
    QPoint iconSize = QPoint(18, 18);
    QPoint point    = rect.topLeft() - QPoint(1, 4);
    QRect pixmapRect(point.x(), point.y(), iconSize.x(), iconSize.y());

    QPixmap statusPixmap = getStatusPixmap(status);
    p.drawPixmap(pixmapRect, statusPixmap);
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::paintListItem(QPainter &p, int index) {
  QRect rect = index2pos(index);
  if (!visibleRegion().intersects(rect)) return;
  if (!getModel()) return;
  QPixmap icon =
      getModel()->getItemData(index, DvItemListModel::Icon).value<QPixmap>();
  QString name =
      getModel()->getItemData(index, DvItemListModel::Name).toString();
  bool isSelected = m_selection->isSelected(index);
  if (isSelected) p.fillRect(rect, QColor(171, 206, 255));
  p.drawPixmap(rect.topLeft(), icon);
  rect.adjust(30, 0, 0, 0);
  p.setPen(Qt::black);
  p.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
             elideText(name, p.font(), rect.width()));
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::paintTableItem(QPainter &p, int index) {
  QRect rect = index2pos(index);
  if (!visibleRegion().intersects(rect)) return;
  if (!getModel()) return;
  bool isSelected = m_selection->isSelected(index);
  if (isSelected)
    p.fillRect(rect, getSelectedItemBackground());
  else if (index % 2 == 0)
    p.fillRect(rect, getAlternateBackground());  // 160,160,160

  DvItemListModel::Status status =
      (DvItemListModel::Status)getModel()
          ->getItemData(index, DvItemListModel::VersionControlStatus)
          .toInt();

  if (getModel()->getItemData(index, DvItemListModel::IsFolder).toBool())
    p.setPen(getFolderTextColor());
  else {
    if (status == DvItemListModel::VC_Missing)
      p.setPen(m_missingColor);
    else
      p.setPen((isSelected) ? getSelectedTextColor() : getTextColor());
  }

  int h  = 0;  // fontMetrics().descent();
  int y  = rect.top();
  int ly = rect.height();
  int x  = rect.left();

  int i, n = (int)m_columns.size();

  // Version Control status pixmap
  QPixmap statusPixmap = getStatusPixmap(status);
  if (!statusPixmap.isNull()) {
    p.drawPixmap(x + 1, y + 1,
                 statusPixmap.scaled(15, 15, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
    x += 15;
  }

  for (i = 0; i < n; i++) {
    if (!(m_columns[i].second.second)) continue;
    DvItemListModel::DataType dataType = m_columns[i].first;
    QString value = getModel()->getItemDataAsString(index, dataType);
    int lx        = m_columns[i].second.first;
    p.drawText(QRect(x + 4, y + 1, lx - 4, ly - 1),
               Qt::AlignLeft | Qt::AlignVCenter,
               elideText(value, p.font(), lx));
    x += lx;
    // If status icon is show, shift the next column left by the width of the
    // icon
    if (i == 0 && !statusPixmap.isNull()) x -= 15;
  }
  if (n > 1) {
    p.setPen(QColor(0, 0, 0, 100));  // column line
    if ((m_columns[0].second.second))
      x = rect.left() + m_columns[0].second.first;
    else
      x = rect.left();
    for (i = 1; i < n; i++) {
      if (!(m_columns[i].second.second)) continue;
      p.drawLine(x - 1, y, x - 1, y + ly);
      x += m_columns[i].second.first;
    }
    p.drawLine(x - 1, y, x - 1, y + ly);
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mousePressEvent(QMouseEvent *event) {
  updateViewParameters(width());
  int index       = pos2index(event->pos());
  bool isSelected = m_selection->isSelected(index);
  m_currentIndex  = index;
  if (event->button() == Qt::RightButton) {
    if (!isSelected) {
      m_selection->selectNone();
      if (!m_isPlayDelegateDisable) m_itemViewPlayDelegate->resetPlayWidget();
      if (0 <= index && index < getItemCount()) m_selection->select(index);
      if (m_globalSelectionEnabled) m_selection->makeCurrent();
      update();
    }
    return;
  } else if (event->button() == Qt::MiddleButton) {
    m_lastMousePos = event->globalPos();
    event->accept();
    return;
  }
  if (!m_isPlayDelegateDisable) {
    QRect rect = index2pos(index);
    QRect iconRect(rect.left() + (rect.width() - m_iconSize.width()) / 2,
                   rect.top(), m_iconSize.width(), m_iconSize.height());
    DvItemListModel *model = getModel();
    if (model->getItemData(index, DvItemListModel::PlayAvailable).toBool() &&
        isSelected) {
      if (m_itemViewPlayDelegate->setPlayWidget(getModel(), index, iconRect,
                                                event->pos())) {
        update();
        return;
      }
    } else
      m_itemViewPlayDelegate->resetPlayWidget();
  }

  // without any modifier, clear the selection
  if (!m_multiSelectionEnabled ||
      (0 == (event->modifiers() & Qt::ControlModifier) &&
       0 == (event->modifiers() & Qt::ShiftModifier) &&
       !m_selection->isSelected(index)))
    m_selection->selectNone();

  // if click something
  if (0 <= index && index < getItemCount()) {
    if (0 != (event->modifiers() & Qt::ControlModifier)) {
      // ctrl-click
      m_selection->select(index, !m_selection->isSelected(index));
    } else if (0 != (event->modifiers() & Qt::ShiftModifier)) {
      // shift-click
      if (!isSelected) {
        int a = index, b = index;
        while (a > 0 && !m_selection->isSelected(a - 1)) a--;
        if (a == 0) a = index;
        int k = getItemCount();
        while (b < k && !m_selection->isSelected(b + 1)) b++;
        if (b == k) b = index;
        int i;
        for (i = a; i <= b; i++) {
          // select except folder items
          if (!getModel()->getItemData(i, DvItemListModel::IsFolder).toBool())
            m_selection->select(i);
        }
      }
    } else {
      m_selection->selectNone();
      m_selection->select(index);
    }
  }
  if (m_globalSelectionEnabled) m_selection->makeCurrent();
  // if (m_viewer ) m_viewer->notifyClick(index);
  m_startDragPosition = event->pos();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() == Qt::MiddleButton) {
    QPoint d       = event->globalPos() - m_lastMousePos;
    m_lastMousePos = event->globalPos();
    if (m_viewer) {
      QScrollBar *scb = m_viewer->verticalScrollBar();
      scb->setValue(scb->value() - d.y());
    }
    return;
  }
  // continuo solo se il bottone sinistro e' premuto.
  else if (!(event->buttons() & Qt::LeftButton))
    return;

  if (!m_isPlayDelegateDisable) {
    int index       = pos2index(event->pos());
    bool isSelected = m_selection->isSelected(index);
    QRect rect      = index2pos(index);
    QRect iconRect(rect.left() + (rect.width() - m_iconSize.width()) / 2,
                   rect.top(), m_iconSize.width(), m_iconSize.height());
    DvItemListModel *model = getModel();
    if (model->getItemData(index, DvItemListModel::PlayAvailable).toBool() &&
        isSelected) {
      if (m_itemViewPlayDelegate->setPlayWidget(getModel(), index, iconRect,
                                                event->pos())) {
        update();
        return;
      }
    } else
      m_itemViewPlayDelegate->resetPlayWidget();
  }

  // faccio partire il drag&drop solo se mi sono mosso di una certa quantita'
  if ((event->pos() - m_startDragPosition).manhattanLength() < 20) return;
  // e se c'e' una selezione non vuota
  if (m_currentIndex < 0 || m_currentIndex >= getItemCount()) return;

  assert(getModel());
  getModel()->startDragDrop();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mouseReleaseEvent(QMouseEvent *event) {
  if (m_viewer)
    m_viewer->notifyClick(m_currentIndex, event->button() == Qt::LeftButton);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::mouseDoubleClickEvent(QMouseEvent *event) {
  int index = pos2index(event->pos());
  if (index < 0 || index >= getItemCount()) return;
  if (m_viewer) m_viewer->notifyDoubleClick(index);
  if (!getModel()->canRenameItem(index)) return;
  QRect captionRect = getCaptionRect(index);
  if (!captionRect.contains(event->pos())) return;
  m_currentIndex = index;

  DVGui::LineEdit *fld = m_editFld;
  // getModel()->refreshData();
  QString name =
      getModel()->getItemData(index, DvItemListModel::Name).toString();
  fld->setText(name);
  fld->setGeometry(captionRect);
  fld->show();
  fld->selectAll();
  fld->setFocus(Qt::OtherFocusReason);
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::rename() {
  QString newName = m_editFld->text();
  m_editFld->hide();
  if (getModel() && 0 <= m_currentIndex && m_currentIndex < getItemCount()) {
    getModel()->renameItem(m_currentIndex, newName);
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::contextMenuEvent(QContextMenuEvent *event) {
  if (!getModel()) return;
  if (m_noContextMenu) return;

  int index   = pos2index(event->pos());
  QMenu *menu = getModel()->getContextMenu(this, index);
  if (menu) {
    menu->exec(event->globalPos());
    delete menu;
  }
}

//-----------------------------------------------------------------------------

bool DvItemViewerPanel::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    // getModel()->refreshData();
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    int index             = pos2index(helpEvent->pos());
    if (0 <= index && index < getItemCount()) {
      QRect rect      = index2pos(index);
      QPoint iconSize = QPoint(18, 18);
      QPoint point    = rect.topLeft() - QPoint(1, 4);
      QRect pixmapRect(point.x(), point.y(), iconSize.x(), iconSize.y());
      if (pixmapRect.contains(helpEvent->pos()))
        QToolTip::showText(helpEvent->globalPos(),
                           getModel()->getItemDataAsString(
                               index, DvItemListModel::VersionControlStatus));
      else {
        QVariant data =
            getModel()->getItemData(index, DvItemListModel::ToolTip);
        if (data == QVariant())
          QToolTip::hideText();
        else
          QToolTip::showText(helpEvent->globalPos(), data.toString());
      }
    } else
      QToolTip::hideText();
  }
  return QWidget::event(event);
}
//-----------------------------------------------------------------------------

void DvItemViewerPanel::setListView() {
  m_viewType = ListView;
  m_viewer->m_windowType == DvItemViewer::Cast ? CastView    = ListView
                                               : BrowserView = ListView;
  emit viewTypeChange(m_viewType);
  m_viewer->updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setTableView() {
  m_viewType = TableView;
  m_viewer->m_windowType == DvItemViewer::Cast ? CastView    = TableView
                                               : BrowserView = TableView;
  emit viewTypeChange(m_viewType);
  m_viewer->updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::setThumbnailsView() {
  m_viewType = ThumbnailView;
  m_viewer->m_windowType == DvItemViewer::Cast ? CastView    = ThumbnailView
                                               : BrowserView = ThumbnailView;
  emit viewTypeChange(m_viewType);
  m_viewer->updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewerPanel::exportFileList() {
  auto project      = TProjectManager::instance()->getCurrentProject();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp;
  if (scene) fp = scene->decodeFilePath(project->getFolder(TProject::Extras));

  QString initialPath;
  if (fp.isEmpty())
    initialPath = QString();
  else
    initialPath = toQString(fp);

  QString fileName = QFileDialog::getSaveFileName(
      0, tr("Save File List"), initialPath, tr("File List (*.csv)"));

  if (fileName.isEmpty()) return;

  QFile data(fileName);
  if (data.open(QFile::WriteOnly)) {
    QTextStream out(&data);

    out << "Name,Frames,Path\n";
    for (int index = 0; index < getItemCount(); index++) {
      if (getModel()->getItemData(index, DvItemListModel::IsFolder).toBool())
        continue;

      for (int i = 0; i < (int)m_columns.size(); i++) {
        DvItemListModel::DataType dataType = m_columns[i].first;

        if (dataType != DvItemListModel::Name &&
            dataType != DvItemListModel::FrameCount &&
            dataType != DvItemListModel::FullPath)
          continue;

        QString value = getModel()->getItemDataAsString(index, dataType);

        out << value;
        if (dataType == DvItemListModel::Name ||
            dataType == DvItemListModel::FrameCount)
          out << ",";
      }
      out << ('\n');
    }
  }
  data.close();
}

//=============================================================================
//
// DvItemViewer
//
//-----------------------------------------------------------------------------

DvItemViewer::DvItemViewer(QWidget *parent, bool noContextMenu,
                           bool multiSelectionEnabled,
                           DvItemViewer::WindowType windowType)
    : QScrollArea(parent), m_model(0) {
  m_windowType = windowType;
  m_panel =
      new DvItemViewerPanel(this, noContextMenu, multiSelectionEnabled, 0);
  setObjectName("BrowserTreeView");
  setStyleSheet("#BrowserTreeView {qproperty-autoFillBackground: true;}");

  setWidget(m_panel);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setAcceptDrops(true);
}

//-----------------------------------------------------------------------------

void DvItemViewer::setModel(DvItemListModel *model) {
  if (model == m_model) return;
  delete m_model;
  m_model = model;
  m_panel->getSelection()->setModel(model);
}

//-----------------------------------------------------------------------------

void DvItemViewer::updateContentSize() {
  int w = m_panel->getContentMinimumWidth();
  if (w < width()) w = width();
  int h = m_panel->getContentHeight(w) +
          20;  // 20 is margin for showing the empty area
  if (h < height()) h = height();
  m_panel->resize(w, h);
}

//-----------------------------------------------------------------------------

void DvItemViewer::resizeEvent(QResizeEvent *event) {
  updateContentSize();
  QScrollArea::resizeEvent(event);
}
//-----------------------------------------------------------------------------

void DvItemViewer::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Home)
    QScrollArea::verticalScrollBar()->setValue(0);
  if (event->key() == Qt::Key_End)
    QScrollArea::verticalScrollBar()->setValue(
        QScrollArea::verticalScrollBar()->maximum());

  QScrollArea::keyPressEvent(event);
}
//-----------------------------------------------------------------------------

void DvItemViewer::resetVerticalScrollBar() {
  QScrollArea::verticalScrollBar()->setValue(0);
}
//-----------------------------------------------------------------------------

void DvItemViewer::dragEnterEvent(QDragEnterEvent *event) {
  const QMimeData *mimeData = event->mimeData();
  if (m_model && m_model->acceptDrop(mimeData)) {
    if (acceptResourceOrFolderDrop(mimeData->urls())) {
      // Force CopyAction
      event->setDropAction(Qt::CopyAction);
      event->accept();
    } else
      event->acceptProposedAction();
  }
}

//-----------------------------------------------------------------------------

void DvItemViewer::dropEvent(QDropEvent *event) {
  const QMimeData *mimeData = event->mimeData();
  if (m_model && m_model->drop(mimeData)) {
    if (acceptResourceOrFolderDrop(mimeData->urls())) {
      // Force CopyAction
      event->setDropAction(Qt::CopyAction);
      event->accept();
    } else
      event->acceptProposedAction();
  }
}

//-----------------------------------------------------------------------------

void DvItemViewer::refresh() {
  updateContentSize();
  update();
}

//-----------------------------------------------------------------------------

void DvItemViewer::selectNone() {
  if (m_panel->getSelection()) m_panel->getSelection()->selectNone();
}

//=============================================================================
//
// DvItemViewTitleBar
//
//-----------------------------------------------------------------------------

DvItemViewerTitleBar::DvItemViewerTitleBar(DvItemViewer *itemViewer,
                                           QWidget *parent, bool isInteractive)
    : QWidget(parent)
    , m_itemViewer(itemViewer)
    , m_isInteractive(isInteractive)
    , m_pos(QPoint(0, 0))
    , m_dragColumnIndex(-1) {
  setMinimumHeight(22);

  bool ret = connect(m_itemViewer->getPanel(),
                     SIGNAL(viewTypeChange(DvItemViewerPanel::ViewType)), this,
                     SLOT(onViewTypeChanged(DvItemViewerPanel::ViewType)));

  assert(ret);

  setMouseTracking(true);
  if ((itemViewer->m_windowType == DvItemViewer::Browser &&
       BrowserView == DvItemViewerPanel::TableView) ||
      (itemViewer->m_windowType == DvItemViewer::Cast &&
       CastView == DvItemViewerPanel::TableView))
    show();
  else
    hide();
}

//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::onViewTypeChanged(
    DvItemViewerPanel::ViewType viewType) {
  if ((m_itemViewer->m_windowType == DvItemViewer::Browser &&
       BrowserView == DvItemViewerPanel::TableView) ||
      (m_itemViewer->m_windowType == DvItemViewer::Cast &&
       CastView == DvItemViewerPanel::TableView))
    show();
  else
    hide();
}

//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::mouseMoveEvent(QMouseEvent *event) {
  QPoint pos = event->pos();
  std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
      columns;
  m_itemViewer->getPanel()->getColumns(columns);
  DvItemListModel *model = m_itemViewer->getModel();

  if (event->buttons() == Qt::NoButton) {
    int i, n = (int)columns.size();
    int x  = 0;
    int ly = height();
    for (i = 0; i < n; i++) {
      if (!(columns[i].second.second)) continue;
      int lx = columns[i].second.first;
      x += lx;
      if (abs(x - pos.x()) > 1) continue;
      m_dragColumnIndex = i;
      setCursor(Qt::SplitHCursor);
      return;
    }
    m_dragColumnIndex = -1;
    setCursor(Qt::ArrowCursor);
  } else if (event->buttons() == Qt::LeftButton && m_dragColumnIndex >= 0) {
    int delta       = pos.x() - m_pos.x();
    int columnWidth = columns[m_dragColumnIndex].second.first;
    if (columnWidth + delta < 20) return;
    m_itemViewer->getPanel()->setColumnWidth(columns[m_dragColumnIndex].first,
                                             columnWidth + delta);
    update();
    m_pos = pos;
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::mousePressEvent(QMouseEvent *event) {
  QPoint pos = event->pos();
  if (event->button() == Qt::LeftButton) {
    if (m_dragColumnIndex >= 0) {
      m_pos = pos;
      return;
    } else
      m_pos = QPoint(0, 0);
    if (!m_isInteractive) return;
    std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
        columns;
    m_itemViewer->getPanel()->getColumns(columns);
    DvItemListModel *model = m_itemViewer->getModel();
    int i, n = (int)columns.size();
    int x  = 0;
    int ly = height();
    for (i = 0; i < n; i++) {
      if (!(columns[i].second.second)) continue;
      int lx = columns[i].second.first;
      QRect columnRect(x, 0, lx, ly - 1);
      x += lx;
      if (!columnRect.contains(pos)) continue;
      DvItemListModel::DataType dataType = columns[i].first;
      model->sortByDataModel(dataType, !model->isDiscendentOrder());
      update();
    }
    return;
  }
  if (event->button() == Qt::RightButton) {
    openContextMenu(event);
  }
}
//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::openContextMenu(QMouseEvent *event) {
  // QAction setNameAction          (QObject::tr("Name"),0);
  // setNameAction.setCheckable(true);
  // setNameAction.setChecked(m_itemViewer->getPanel()->getVisibility(DvItemListModel::Name));
  QAction setSizeAction(QObject::tr("Size"), 0);
  setSizeAction.setCheckable(true);
  setSizeAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileSize));
  QAction setFramesAction(QObject::tr("Frames"), 0);
  setFramesAction.setCheckable(true);
  setFramesAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::FrameCount));
  QAction setDateCreatedAction(QObject::tr("Date Created"), 0);
  setDateCreatedAction.setCheckable(true);
  setDateCreatedAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::CreationDate));
  QAction setDateModifiedAction(QObject::tr("Date Modified"), 0);
  setDateModifiedAction.setCheckable(true);
  setDateModifiedAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::ModifiedDate));
  QAction setTypeAction(QObject::tr("Type"), 0);
  setTypeAction.setCheckable(true);
  setTypeAction.setChecked(
      m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileType));
  QAction setVersionControlAction(QObject::tr("Version Control"), 0);
  setVersionControlAction.setCheckable(true);
  setVersionControlAction.setChecked(m_itemViewer->getPanel()->getVisibility(
      DvItemListModel::VersionControlStatus));
  QMenu menu(0);
  // menu.addAction(&setNameAction);
  menu.addAction(&setFramesAction);
  if (m_itemViewer->m_windowType == DvItemViewer::Browser) {
    menu.addAction(&setSizeAction);
    menu.addAction(&setDateCreatedAction);
    menu.addAction(&setDateModifiedAction);
    menu.addAction(&setTypeAction);
    menu.addAction(&setVersionControlAction);
  }

  QAction *action = menu.exec(event->globalPos());  // QCursor::pos());
  // if(action==&setNameAction)
  // m_itemViewer->getPanel()->setVisibility(DvItemListModel::Name,!m_itemViewer->getPanel()->getVisibility(DvItemListModel::Name));
  if (action == &setSizeAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::FileSize,
        !m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileSize));
  if (action == &setFramesAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::FrameCount,
        !m_itemViewer->getPanel()->getVisibility(DvItemListModel::FrameCount));
  if (action == &setDateCreatedAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::CreationDate, !m_itemViewer->getPanel()->getVisibility(
                                           DvItemListModel::CreationDate));
  if (action == &setDateModifiedAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::ModifiedDate, !m_itemViewer->getPanel()->getVisibility(
                                           DvItemListModel::ModifiedDate));
  if (action == &setTypeAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::FileType,
        !m_itemViewer->getPanel()->getVisibility(DvItemListModel::FileType));
  if (action == &setVersionControlAction)
    m_itemViewer->getPanel()->setVisibility(
        DvItemListModel::VersionControlStatus,
        !m_itemViewer->getPanel()->getVisibility(
            DvItemListModel::VersionControlStatus));

  m_itemViewer->getPanel()->update();
}
//-----------------------------------------------------------------------------

void DvItemViewerTitleBar::paintEvent(QPaintEvent *) {
  QPainter p(this);
  std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
      columns;
  m_itemViewer->getPanel()->getColumns(columns);
  QRect rect(0, 0, width(), height());

  QBrush nb    = QBrush(Qt::NoBrush);
  QPalette pal = QPalette(nb, nb, QBrush(QColor(getColBorderColor())),
                          QBrush(QColor(getColBorderColor())),
                          QBrush(QColor(Qt::gray)), nb, nb, nb, nb);

  p.fillRect(rect, getColColor());

  p.setPen(getColTextColor());
  int h  = 0;  // fontMetrics().descent();
  int y  = rect.top();
  int ly = rect.height();
  int lx = rect.width();
  int x  = rect.left();

  DvItemListModel *model = m_itemViewer->getModel();
  int i, n = (int)columns.size();
  for (i = 0; i < n; i++) {
    if (!(columns[i].second.second)) continue;
    DvItemListModel::DataType dataType = columns[i].first;
    int columnLx                       = columns[i].second.first;

    // paint background
    QColor bgColor;
    if (dataType == model->getCurrentOrderType())
      bgColor = QColor(getColSortedColor());
    else
      bgColor = QColor(0, 0, 0, 0);

    QRect typeRect(x - 1, y - 1, columnLx + 1, ly + 1);
    QBrush brush(bgColor);
    qDrawShadePanel(&p, typeRect, pal, false, 1, &brush);

    // draw ordering arrow
    if (m_isInteractive && dataType == model->getCurrentOrderType()) {
      QIcon arrowIcon;
      if (model->isDiscendentOrder())
        arrowIcon = createQIconPNG("arrow_up");
      else
        arrowIcon = createQIconPNG("arrow_down");
      p.drawPixmap(QRect(x + columnLx - 11, y + 6, 8, 8),
                   arrowIcon.pixmap(8, 8));
    }

    // draw text
    QString value = model->getItemDataIdentifierName(dataType);
    p.drawText(QRect(x + 4, y + 1, columnLx - 4, ly - 1),
               Qt::AlignLeft | Qt::AlignVCenter,
               elideText(value, p.font(), columnLx - 4));

    x += columnLx;
  }
}

//=============================================================================
//
// DvItemViewButtonBar
//
//-----------------------------------------------------------------------------

DvItemViewerButtonBar::DvItemViewerButtonBar(DvItemViewer *itemViewer,
                                             QWidget *parent)
    : QToolBar(parent)
    , m_itemViewer(itemViewer)
    , m_leftSpacerAct(nullptr)
    , m_rightSpacerAct(nullptr)
    , m_advancedSep(nullptr)
    , m_bgSep(nullptr)
    , m_typeSep(nullptr)
    , m_typeLevelSep(nullptr)
    , m_searchSep(nullptr)
    , m_bgWhiteAct(nullptr)
    , m_bgBlackAct(nullptr)
    , m_bgTransparentAct(nullptr)
    , m_bgCheckeredAct(nullptr)
    , m_sizeMenuAct(nullptr)
    , m_infoPanelAct(nullptr)
    , m_sizeSliderAct(nullptr)
    , m_fpsAct(nullptr)
    , m_typeFilterListAct(nullptr)
    , m_favoritesFilterAct(nullptr)
    , m_searchAct(nullptr)
    , m_projectFoldersSep(nullptr)
    , m_projectFolderHostAct(nullptr)
    , m_bgGroup(nullptr)
    , m_sizeMenuBtn(nullptr)
    , m_typeFilterListBtn(nullptr)
    , m_favoritesFilterBtn(nullptr)
    , m_fpsBtn(nullptr)
    , m_fpsField(nullptr)
    , m_loopBtn(nullptr)
    , m_sizeSlider(nullptr)
    , m_searchEdit(nullptr)
    , m_projectFolderHost(nullptr)
    , m_advancedDisplayAct(nullptr)
    , m_guiPartsFlag(AGUI_All)
    , m_updatingUi(false) {
  setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  setObjectName("buttonBar");
  setIconSize(QSize(16, 16));
  setContextMenuPolicy(Qt::DefaultContextMenu);

  QIcon backButtonIcon = createQIcon("fb_back");
  QIcon fwdButtonIcon  = createQIcon("fb_fwd");

  m_folderBack = new QAction(backButtonIcon, tr("Back"), this);
  m_folderBack->setIconText("");
  addAction(m_folderBack);
  m_folderFwd = new QAction(fwdButtonIcon, tr("Forward"), this);
  m_folderFwd->setIconText("");
  addAction(m_folderFwd);

  QIcon folderUpIcon = createQIcon("fb_up");
  QAction *folderUp  = new QAction(folderUpIcon, tr("Up One Level"), this);
  folderUp->setIconText(tr("Up"));
  addAction(folderUp);
  addSeparator();

  QIcon newFolderIcon = createQIcon("folder_new");
  QAction *newFolder  = new QAction(newFolderIcon, tr("New Folder"), this);
  newFolder->setIconText(tr("New"));
  addAction(newFolder);
  addSeparator();

  // view mode
  QActionGroup *actions = new QActionGroup(this);
  actions->setExclusive(true);

  QIcon thumbViewIcon = createQIcon("viewicon");
  QAction *thumbView  = new QAction(thumbViewIcon, tr("Icons View"), this);
  thumbView->setCheckable(true);
  thumbView->setIconText(tr("Icon"));
  thumbView->setChecked((itemViewer->m_windowType == DvItemViewer::Browser &&
                         DvItemViewerPanel::ThumbnailView == BrowserView) ||
                        (itemViewer->m_windowType == DvItemViewer::Cast &&
                         DvItemViewerPanel::ThumbnailView == CastView));
  actions->addAction(thumbView);
  addAction(thumbView);

  QIcon listViewIcon = createQIcon("viewlist");
  QAction *listView  = new QAction(listViewIcon, tr("List View"), this);
  listView->setCheckable(true);
  listView->setIconText(tr("List"));
  listView->setChecked((itemViewer->m_windowType == DvItemViewer::Browser &&
                        DvItemViewerPanel::TableView == BrowserView) ||
                       (itemViewer->m_windowType == DvItemViewer::Cast &&
                        DvItemViewerPanel::TableView == CastView));
  actions->addAction(listView);
  addAction(listView);

  addSeparator();

  // button to export file list to csv
  QAction *exportFileListAction = new QAction(tr("Export File List"), this);
  exportFileListAction->setIcon(createQIcon("export"));
  addAction(exportFileListAction);

  if (itemViewer->m_windowType == DvItemViewer::Browser &&
      !Preferences::instance()->isWatchFileSystemEnabled()) {
    addAction(CommandManager::instance()->getAction("MI_RefreshTree"));
    addSeparator();
  }

  connect(exportFileListAction, SIGNAL(triggered()), itemViewer->getPanel(),
          SLOT(exportFileList()));

  connect(folderUp, SIGNAL(triggered()), SIGNAL(folderUp()));
  connect(newFolder, SIGNAL(triggered()), SIGNAL(newFolder()));
  connect(thumbView, SIGNAL(triggered()), itemViewer->getPanel(),
          SLOT(setThumbnailsView()));
  connect(listView, SIGNAL(triggered()), itemViewer->getPanel(),
          SLOT(setTableView()));

  connect(m_folderBack, SIGNAL(triggered()), SIGNAL(folderBack()));
  connect(m_folderFwd, SIGNAL(triggered()), SIGNAL(folderFwd()));

  if (itemViewer->m_windowType == DvItemViewer::Browser) {
    connect(TApp::instance()->getCurrentScene(),
            SIGNAL(preferenceChanged(const QString &)), this,
            SLOT(onPreferenceChanged(const QString &)));
  }

  // BG modes, size presets, size slider (right-click to toggle).
  buildAdvancedControls();

  DvItemViewerPanel *panel = itemViewer->getPanel();
  connect(panel, &DvItemViewerPanel::thumbnailSizeChanged, this,
          &DvItemViewerButtonBar::onPanelThumbnailSizeChanged);
  // Layout updates immediately; HD regen is debounced in the panel.
  connect(panel, &DvItemViewerPanel::thumbnailSizeChanged, itemViewer,
          [itemViewer](const QSize &) { itemViewer->refresh(); });

  const bool isCast = itemViewer->m_windowType == DvItemViewer::Cast;
  const bool advanced =
      isCast ? (bool)CastAdvancedDisplay : (bool)BrowserAdvancedDisplay;
  m_guiPartsFlag = (unsigned int)(isCast ? (int)CastAdvancedGuiParts
                                         : (int)BrowserAdvancedGuiParts);
  if (m_guiPartsFlag == 0) m_guiPartsFlag = AGUI_All;
  m_guiPartsFlag &= AGUI_All;  // drop legacy per-type filter visibility bits
  if (m_advancedDisplayAct) {
    m_advancedDisplayAct->blockSignals(true);
    m_advancedDisplayAct->setChecked(advanced);
    m_advancedDisplayAct->blockSignals(false);
  }
  refreshAdvancedControlsVisibility();
  syncAdvancedControlsFromPanel();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onHistoryChanged(bool backEnable, bool fwdEnable) {
  if (backEnable)
    m_folderBack->setEnabled(true);
  else
    m_folderBack->setEnabled(false);

  if (fwdEnable)
    m_folderFwd->setEnabled(true);
  else
    m_folderFwd->setEnabled(false);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onPreferenceChanged(const QString &prefName) {
  if (prefName == "CurrentStyleSheetName" && m_searchEdit)
    applyBrowserSearchPlaceholderStyle(m_searchEdit);

  // react only when the related preference is changed
  if (prefName != "WatchFileSystem") return;

  QAction *refreshAct = CommandManager::instance()->getAction("MI_RefreshTree");
  if (Preferences::instance()->isWatchFileSystemEnabled()) {
    removeAction(refreshAct);
    removeAction(actions().last());  // remove separator
  } else {
    addAction(refreshAct);
    addSeparator();
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::makeActionIconOnly(QAction *action) {
  if (!action) return;
  // Keep a tooltip (= action text) and hide the label next to the icon.
  if (action->toolTip().isEmpty()) action->setToolTip(action->text());
  if (QWidget *w = widgetForAction(action)) {
    if (auto *tb = qobject_cast<QToolButton *>(w)) {
      tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
      tb->setToolTip(action->toolTip());
    }
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::styleAdvancedIconWidget(QWidget *widget) {
  auto *tb = qobject_cast<QToolButton *>(widget);
  if (!tb) return;
  tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
  tb->setAutoRaise(true);
  tb->setFocusPolicy(Qt::NoFocus);
  tb->setIconSize(QSize(16, 16));
  tb->setFixedSize(22, 22);
  tb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  tb->setStyleSheet(
      QStringLiteral("QToolButton { padding: 2px; margin: 0px; }"));
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::styleAdvancedIconButton(QAction *action) {
  if (!action) return;
  if (action->toolTip().isEmpty()) action->setToolTip(action->text());
  action->setIconText(QString());
  makeActionIconOnly(action);
  styleAdvancedIconWidget(widgetForAction(action));
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::uniformAdvancedIconButtons() {
  styleAdvancedIconButton(m_bgWhiteAct);
  styleAdvancedIconButton(m_bgBlackAct);
  styleAdvancedIconButton(m_bgTransparentAct);
  styleAdvancedIconButton(m_bgCheckeredAct);
  styleAdvancedIconWidget(m_sizeMenuBtn);
  styleAdvancedIconWidget(m_typeFilterListBtn);
  styleAdvancedIconWidget(m_fpsBtn);
  styleAdvancedIconButton(m_infoPanelAct);
  for (QAction *act : m_typeFilterActs) styleAdvancedIconButton(act);
}

//-----------------------------------------------------------------------------

bool DvItemViewerButtonBar::isAdvancedDisplayOn() const {
  return m_itemViewer && m_itemViewer->getPanel() &&
         m_itemViewer->getPanel()->isAdvancedDisplay();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::refreshAdvancedControlsVisibility() {
  const bool advancedOn = isAdvancedDisplayOn();
  const bool isBrowser =
      m_itemViewer && m_itemViewer->m_windowType == DvItemViewer::Browser;

  auto partOn = [this, advancedOn](unsigned int part) {
    return advancedOn && (m_guiPartsFlag & part);
  };

  const bool showSlider = partOn(AGUI_SizeSlider);
  const bool showMenu   = partOn(AGUI_SizeMenu);
  const bool showBg     = partOn(AGUI_Background);
  const bool showTypeIcons = partOn(AGUI_TypeFilters) && isBrowser;
  const bool showTypeList  = partOn(AGUI_TypeFilterList) && isBrowser;
  const bool showTypes     = showTypeIcons || showTypeList;
  const bool showSearch    = partOn(AGUI_Search) && isBrowser;
  const bool showFavorites = partOn(AGUI_Favorites) && isBrowser;
  const bool showFps       = partOn(AGUI_PlayFps) && isBrowser;
  const bool showProjectFolders = partOn(AGUI_ProjectFolders) && isBrowser;
  const bool showInfoPanel      = partOn(AGUI_InfoPanel) && isBrowser;
  const bool anyPart = showSlider || showMenu || showBg || showTypes || showSearch ||
                       showFavorites || showFps || showProjectFolders ||
                       showInfoPanel;

  // Keep the advanced cluster centered (spacers left and right).
  if (m_leftSpacerAct) m_leftSpacerAct->setVisible(anyPart);
  if (m_rightSpacerAct) m_rightSpacerAct->setVisible(anyPart);
  if (m_advancedSep) m_advancedSep->setVisible(anyPart);
  if (m_sizeSliderAct) m_sizeSliderAct->setVisible(showSlider);
  if (m_sizeMenuAct) m_sizeMenuAct->setVisible(showMenu);
  if (m_infoPanelAct) m_infoPanelAct->setVisible(showInfoPanel);
  if (m_bgSep) m_bgSep->setVisible(showMenu && showBg);
  if (m_bgWhiteAct) m_bgWhiteAct->setVisible(showBg);
  if (m_bgBlackAct) m_bgBlackAct->setVisible(showBg);
  if (m_bgTransparentAct) m_bgTransparentAct->setVisible(showBg);
  if (m_bgCheckeredAct) m_bgCheckeredAct->setVisible(showBg);
  if (m_typeSep) m_typeSep->setVisible(showBg && showTypes);
  if (m_typeLevelSep) m_typeLevelSep->setVisible(showTypeIcons);
  for (QAction *act : m_typeFilterActs) act->setVisible(showTypeIcons);
  // QToolBar: must hide via QAction, not QWidget::setVisible.
  for (QAction *gap : m_typeFilterIconGaps) {
    if (gap) gap->setVisible(showTypeIcons);
  }
  if (m_typeFilterListAct) m_typeFilterListAct->setVisible(showTypeList);
  if (m_typeFilterListGap) m_typeFilterListGap->setVisible(showTypeList);
  const bool showAfterTypes =
      showTypeList || showFavorites || showFps || showSearch;
  if (m_searchSep)
    m_searchSep->setVisible(showTypeIcons && showAfterTypes);
  if (m_favoritesFilterAct) m_favoritesFilterAct->setVisible(showFavorites);
  if (m_favoritesFilterGap) m_favoritesFilterGap->setVisible(showFavorites);
  if (m_fpsAct) m_fpsAct->setVisible(showFps);
  if (m_fpsGap) m_fpsGap->setVisible(showFps);
  if (m_searchAct) m_searchAct->setVisible(showSearch);

  // Project-folder shortcuts follow Advanced Display + Show/Hide.
  refreshProjectFolderShortcuts();

  if (anyPart) uniformAdvancedIconButtons();
}

//-----------------------------------------------------------------------------

namespace {
// Project subfolder order (see stuff/profiles/project_folders.txt).
const char *kProjectFolderOrder[] = {"drawings", "extras",  "inputs", "outputs",
                                     "palettes", "scenes", "scripts"};
constexpr int kProjectFolderCount =
    int(sizeof(kProjectFolderOrder) / sizeof(kProjectFolderOrder[0]));

void clearLayoutWidgets(QLayout *layout) {
  if (!layout) return;
  while (QLayoutItem *item = layout->takeAt(0)) {
    if (QWidget *w = item->widget()) w->deleteLater();
    delete item;
  }
}

QString folderMonogramLabel(const QString &folderName) {
  if (folderName == QLatin1String("scripts")) return QStringLiteral("SC");
  if (folderName.isEmpty()) return QString();
  return folderName.left(1).toUpper();
}
}  // namespace

void DvItemViewerButtonBar::setInfoPanelChecked(bool checked) {
  if (!m_infoPanelAct) return;
  m_infoPanelAct->blockSignals(true);
  m_infoPanelAct->setChecked(checked);
  m_infoPanelAct->blockSignals(false);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::setInfoPanelEnabled(bool enabled) {
  if (m_infoPanelAct) m_infoPanelAct->setEnabled(enabled);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::refreshProjectFolderShortcuts() {
  QVector<QPair<QString, TFilePath>> folders;
  auto project = TProjectManager::instance()->getCurrentProject();
  if (project) {
    for (int i = 0; i < kProjectFolderCount; ++i) {
      const std::string name = kProjectFolderOrder[i];
      const TFilePath fp     = project->getFolder(name, true);
      if (!TFileStatus(fp).doesExist()) continue;
      folders.append({QString::fromStdString(name), fp});
    }
  }
  setProjectFolderShortcuts(folders);
}

void DvItemViewerButtonBar::setProjectFolderShortcuts(
    const QVector<QPair<QString, TFilePath>> &folders) {
  if (!m_projectFolderHost || !m_projectFolderHostAct) return;

  const bool isBrowser =
      m_itemViewer && m_itemViewer->m_windowType == DvItemViewer::Browser;
  const bool show = isBrowser && isAdvancedDisplayOn() &&
                    (m_guiPartsFlag & AGUI_ProjectFolders) && !folders.isEmpty();

  clearLayoutWidgets(m_projectFolderHost->layout());

  if (show) {
    auto *layout = m_projectFolderHost->layout();
    for (const auto &entry : folders) {
      auto *btn = new ProjectFolderShortcutButton(
          folderMonogramLabel(entry.first), m_projectFolderHost);
      btn->setToolTip(entry.first);
      styleAdvancedIconWidget(btn);
      btn->setIconSize(QSize(22, 22));
      btn->setFixedSize(26, 26);
      btn->setStyleSheet(
          QStringLiteral("QToolButton { padding: 0px; margin: 0px; }"));
      const TFilePath path = entry.second;
      connect(btn, &QToolButton::clicked, this,
              [this, path]() { emit projectFolderTriggered(path); });
      layout->addWidget(btn);
    }
    m_projectFolderHost->adjustSize();
  }

  if (m_projectFoldersSep) m_projectFoldersSep->setVisible(show);
  m_projectFolderHostAct->setVisible(show);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::buildAdvancedControls() {
  auto makeSpacer = [this]() {
    QWidget *w = new QWidget(this);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    w->setMinimumWidth(0);
    return w;
  };
  auto addBlockSep = [this]() -> QAction * {
    QWidget *wrap = new QWidget(this);
    wrap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto *lay = new QHBoxLayout(wrap);
    lay->setContentsMargins(kBrowserBlockSepPad, 0, kBrowserBlockSepPad, 0);
    lay->setSpacing(0);
    auto *line = new BrowserBlockSepLine(wrap);
    lay->addWidget(line, 0, Qt::AlignVCenter);
    return addWidget(wrap);
  };
  // Store the QAction: QToolBar ignores QWidget::setVisible on addWidget items.
  auto addIconGap = [this]() -> QAction * {
    QWidget *gap = new QWidget(this);
    gap->setFixedWidth(kBrowserIconGap);
    gap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    return addWidget(gap);
  };

  m_leftSpacerAct = addWidget(makeSpacer());
  m_advancedSep   = addBlockSep();

  QWidget *sliderWrap = new QWidget(this);
  sliderWrap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  QHBoxLayout *lay = new QHBoxLayout(sliderWrap);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  auto *zoomOutBtn = new QToolButton(sliderWrap);
  zoomOutBtn->setIcon(createQIcon("zoom_out"));
  zoomOutBtn->setToolTip(tr("Smaller thumbnails"));
  zoomOutBtn->setAutoRaise(true);
  zoomOutBtn->setFocusPolicy(Qt::NoFocus);
  zoomOutBtn->setIconSize(QSize(15, 15));
  zoomOutBtn->setFixedSize(QSize(16, 16));
  zoomOutBtn->setStyleSheet(
      QStringLiteral("QToolButton { padding: 0; margin: 0; border: none; }"));
  lay->addWidget(zoomOutBtn, 0, Qt::AlignVCenter);

  m_sizeSlider = new QSlider(Qt::Horizontal, sliderWrap);
  m_sizeSlider->setObjectName("browserThumbSizeSlider");
  m_sizeSlider->setRange(kBrowserThumbMinWidth, kBrowserThumbMaxWidth);
  m_sizeSlider->setValue(kBrowserThumbDefaultW);
  m_sizeSlider->setFixedWidth(81);
  m_sizeSlider->setFixedHeight(16);
  m_sizeSlider->setToolTip(tr("Thumbnail Size"));
  m_sizeSlider->setFocusPolicy(Qt::ClickFocus);
  m_sizeSlider->setStyleSheet(QStringLiteral(
      "QSlider#browserThumbSizeSlider { padding: 0; margin: 0; }"
      "QSlider#browserThumbSizeSlider::groove:horizontal {"
      "  height: 3px; margin: 0; }"
      "QSlider#browserThumbSizeSlider::handle:horizontal {"
      "  width: 10px; margin: -5px 0; }"));
  lay->addWidget(m_sizeSlider, 0, Qt::AlignVCenter);

  auto *zoomInBtn = new QToolButton(sliderWrap);
  zoomInBtn->setIcon(createQIcon("zoom_in"));
  zoomInBtn->setToolTip(tr("Larger thumbnails"));
  zoomInBtn->setAutoRaise(true);
  zoomInBtn->setFocusPolicy(Qt::NoFocus);
  zoomInBtn->setIconSize(QSize(15, 15));
  zoomInBtn->setFixedSize(QSize(16, 16));
  zoomInBtn->setStyleSheet(
      QStringLiteral("QToolButton { padding: 0; margin: 0; border: none; }"));
  lay->addWidget(zoomInBtn, 0, Qt::AlignVCenter);

  sliderWrap->setLayout(lay);
  m_sizeSliderAct = addWidget(sliderWrap);

  connect(m_sizeSlider, &QSlider::valueChanged, this,
          &DvItemViewerButtonBar::onSizeSliderChanged);
  connect(zoomOutBtn, &QToolButton::clicked, this, [this]() {
    if (!m_sizeSlider) return;
    m_sizeSlider->setValue(m_sizeSlider->value() - kBrowserIconQuantStep);
  });
  connect(zoomInBtn, &QToolButton::clicked, this, [this]() {
    if (!m_sizeSlider) return;
    m_sizeSlider->setValue(m_sizeSlider->value() + kBrowserIconQuantStep);
  });

  m_sizeMenuBtn = new QToolButton(this);
  m_sizeMenuBtn->setIcon(createQIcon("menu"));
  m_sizeMenuBtn->setToolTip(tr("View Mode"));
  m_sizeMenuBtn->setPopupMode(QToolButton::InstantPopup);
  m_sizeMenuBtn->setAutoRaise(true);
  m_sizeMenuBtn->setFocusPolicy(Qt::NoFocus);
  m_sizeMenuBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

  QMenu *sizeMenu = new QMenu(m_sizeMenuBtn);
  auto addPreset  = [sizeMenu](const QString &label, int preset) {
    QAction *act = sizeMenu->addAction(label);
    act->setData(preset);
    return act;
  };
  addPreset(tr("List"), (int)DvItemViewerPanel::SizeList);
  sizeMenu->addSeparator();
  addPreset(tr("Small"), (int)DvItemViewerPanel::SizeSmall);
  addPreset(tr("Medium"), (int)DvItemViewerPanel::SizeMedium);
  addPreset(tr("Large"), (int)DvItemViewerPanel::SizeLarge);
  addPreset(tr("Extra Large"), (int)DvItemViewerPanel::SizeExtraLarge);
  addPreset(tr("Huge"), (int)DvItemViewerPanel::SizeHuge);
  m_sizeMenuBtn->setMenu(sizeMenu);
  connect(sizeMenu, &QMenu::triggered, this,
          &DvItemViewerButtonBar::onSizePresetTriggered);
  m_sizeMenuAct = addWidget(m_sizeMenuBtn);
  addIconGap();

  m_bgSep = addBlockSep();

  m_bgGroup = new QActionGroup(this);
  m_bgGroup->setExclusive(false);  // re-click or uncheck all → Auto

  m_bgWhiteAct = new QAction(createQIcon("browser_preview_white"),
                             tr("White Background"), this);
  m_bgWhiteAct->setCheckable(true);
  m_bgWhiteAct->setData((int)DvItemViewerPanel::BgWhite);
  m_bgWhiteAct->setToolTip(tr("White Background"));
  m_bgGroup->addAction(m_bgWhiteAct);
  addAction(m_bgWhiteAct);
  addIconGap();

  m_bgBlackAct = new QAction(createQIcon("browser_preview_black"),
                             tr("Black Background"), this);
  m_bgBlackAct->setCheckable(true);
  m_bgBlackAct->setData((int)DvItemViewerPanel::BgBlack);
  m_bgBlackAct->setToolTip(tr("Black Background"));
  m_bgGroup->addAction(m_bgBlackAct);
  addAction(m_bgBlackAct);
  addIconGap();

  m_bgTransparentAct =
      new QAction(createQIcon("browser_preview_transparency"),
                  tr("Transparent Background"), this);
  m_bgTransparentAct->setCheckable(true);
  m_bgTransparentAct->setData((int)DvItemViewerPanel::BgTransparent);
  m_bgTransparentAct->setToolTip(tr("Transparent Background"));
  m_bgGroup->addAction(m_bgTransparentAct);
  addAction(m_bgTransparentAct);
  addIconGap();

  m_bgCheckeredAct = new QAction(createQIcon("browser_preview_checkboard"),
                                 tr("Checkered Background"), this);
  m_bgCheckeredAct->setCheckable(true);
  m_bgCheckeredAct->setData((int)DvItemViewerPanel::BgCheckered);
  m_bgCheckeredAct->setToolTip(tr("Checkered Background"));
  m_bgGroup->addAction(m_bgCheckeredAct);
  addAction(m_bgCheckeredAct);
  addIconGap();

  connect(m_bgGroup, &QActionGroup::triggered, this,
          &DvItemViewerButtonBar::onBgModeTriggered);

  m_typeSep = addBlockSep();

  static const QStringList kRasterExts = {
      "PNG", "TIF", "TIFF", "JPG", "JPEG", "TGA", "BMP", "EXR"};
  static const QStringList kMediaExts = {
      "MP4", "MOV", "AVI", "WEBM", "GIF", "3GP"};
  static const QStringList kAudioExts = {
      "WAV", "AIFF", "AIF", "MP3", "FLAC", "OGG"};

  struct TypeDef {
    const char *iconName;
    const char *label;
    QStringList extensions;
  };
  const TypeDef levelDefs[] = {
      {"browser_image_file", QT_TR_NOOP("Raster Image"), kRasterExts},
      {"browser_toonz_raster_file", QT_TR_NOOP("Toonz Raster (TLV)"), {"TLV"}},
      {"browser_toonz_vector_file", QT_TR_NOOP("Toonz Vector (PLI)"), {"PLI"}},
      {"browser_palette", QT_TR_NOOP("Palette (TPL)"), {"TPL"}},
  };
  const TypeDef assetDefs[] = {
      {"browser_psd_file", QT_TR_NOOP("PSD"), {"PSD"}},
      {"browser_svg_file", QT_TR_NOOP("SVG"), {"SVG"}},
      {"browser_media_file", QT_TR_NOOP("Media"), kMediaExts},
      {"browser_audio_file", QT_TR_NOOP("Audio"), kAudioExts},
  };

  auto addTypeFilter = [this, &addIconGap](const TypeDef &def) {
    const QString label = tr(def.label);
    QAction *act =
        new QAction(createQIcon(QLatin1String(def.iconName)), label, this);
    act->setCheckable(true);
    act->setData(def.extensions);
    act->setToolTip(label);
    addAction(act);
    m_typeFilterIconGaps.append(addIconGap());
    connect(act, &QAction::triggered, this,
            &DvItemViewerButtonBar::onTypeFilterTriggered);
    m_typeFilterActs.append(act);
  };

  m_typeFilterActs.clear();
  m_typeFilterMenuActs.clear();
  m_typeFilterIconGaps.clear();
  for (const TypeDef &def : levelDefs) addTypeFilter(def);

  m_typeLevelSep = addBlockSep();

  for (const TypeDef &def : assetDefs) addTypeFilter(def);

  m_searchSep = addBlockSep();

  m_typeFilterListBtn = new QToolButton(this);
  m_typeFilterListBtn->setIcon(createQIcon("browser_filterlist"));
  m_typeFilterListBtn->setToolTip(tr("Content Type Filters"));
  m_typeFilterListBtn->setPopupMode(QToolButton::InstantPopup);
  m_typeFilterListBtn->setAutoRaise(true);
  m_typeFilterListBtn->setFocusPolicy(Qt::NoFocus);
  m_typeFilterListBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  QMenu *typeFilterMenu = new QMenu(m_typeFilterListBtn);
  for (QAction *iconAct : m_typeFilterActs) {
    QAction *menuAct = typeFilterMenu->addAction(iconAct->text());
    menuAct->setCheckable(true);
    connect(menuAct, &QAction::triggered, this,
            &DvItemViewerButtonBar::onTypeFilterMenuTriggered);
    m_typeFilterMenuActs.append(menuAct);
  }
  connect(typeFilterMenu, &QMenu::aboutToShow, this,
          &DvItemViewerButtonBar::syncTypeFilterMenuFromIcons);
  m_typeFilterListBtn->setMenu(typeFilterMenu);
  m_typeFilterListAct = addWidget(m_typeFilterListBtn);
  m_typeFilterListGap = addIconGap();

  m_favoritesFilterBtn = new QToolButton(this);
  m_favoritesFilterBtn->setIcon(createQIcon("star"));
  m_favoritesFilterBtn->setToolTip(tr("Favorites Only"));
  m_favoritesFilterBtn->setCheckable(true);
  m_favoritesFilterBtn->setAutoRaise(true);
  m_favoritesFilterBtn->setFocusPolicy(Qt::NoFocus);
  m_favoritesFilterBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  styleAdvancedIconWidget(m_favoritesFilterBtn);
  connect(m_favoritesFilterBtn, &QToolButton::toggled, this,
          &DvItemViewerButtonBar::onFavoritesFilterToggled);
  m_favoritesFilterAct = addWidget(m_favoritesFilterBtn);
  m_favoritesFilterGap = addIconGap();

  m_fpsBtn = new QToolButton(this);
  m_fpsBtn->setIcon(createQIcon("browser_play_fps"));
  m_fpsBtn->setToolTip(tr("Playback FPS"));
  m_fpsBtn->setPopupMode(QToolButton::InstantPopup);
  m_fpsBtn->setAutoRaise(true);
  m_fpsBtn->setFocusPolicy(Qt::NoFocus);
  m_fpsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  QMenu *fpsMenu = new QMenu(m_fpsBtn);

  auto *fpsTopHost = new QWidget(fpsMenu);
  auto *fpsTopLay  = new QHBoxLayout(fpsTopHost);
  fpsTopLay->setContentsMargins(8, 6, 8, 4);
  fpsTopLay->setSpacing(6);
  m_fpsField = new DVGui::IntLineEdit(fpsTopHost, 10, 1, 120);
  m_fpsField->setPlaceholderText(tr("Enter FPS…"));
  m_fpsField->setFixedWidth(88);
  m_fpsField->setToolTip(tr("Playback speed (frames per second)"));
  fpsTopLay->addWidget(m_fpsField);
  m_loopBtn = new QToolButton(fpsTopHost);
  m_loopBtn->setIcon(createQIcon("loop"));
  m_loopBtn->setCheckable(true);
  m_loopBtn->setChecked(true);
  m_loopBtn->setAutoRaise(true);
  m_loopBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  m_loopBtn->setIconSize(QSize(16, 16));
  m_loopBtn->setFixedSize(22, 22);
  m_loopBtn->setToolTip(tr("Loop"));
  fpsTopLay->addWidget(m_loopBtn);
  auto *fpsTopAct = new QWidgetAction(fpsMenu);
  fpsTopAct->setDefaultWidget(fpsTopHost);
  fpsMenu->addAction(fpsTopAct);
  fpsMenu->addSeparator();

  const int fpsChoices[] = {6, 8, 10, 12, 24, 30};
  for (int fps : fpsChoices) {
    QAction *act = fpsMenu->addAction(tr("%1 FPS").arg(fps));
    act->setData(fps);
    act->setCheckable(true);
  }
  connect(m_fpsField, &QLineEdit::editingFinished, this,
          &DvItemViewerButtonBar::onPlayFpsFieldEdited);
  connect(m_loopBtn, &QToolButton::toggled, this,
          &DvItemViewerButtonBar::onPlayLoopToggled);
  m_fpsBtn->setMenu(fpsMenu);
  connect(fpsMenu, &QMenu::triggered, this,
          &DvItemViewerButtonBar::onPlayFpsTriggered);
  connect(fpsMenu, &QMenu::aboutToShow, this, [this]() {
    if (m_fpsField) {
      m_fpsField->setFocus(Qt::OtherFocusReason);
      m_fpsField->selectAll();
    }
  });
  m_fpsAct = addWidget(m_fpsBtn);
  m_fpsGap = addIconGap();
  updateFpsButtonLabel();

  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setObjectName(QStringLiteral("browserSearchEdit"));
  m_searchEdit->setPlaceholderText(tr("Search…"));
  m_searchEdit->setClearButtonEnabled(true);
  m_searchEdit->setFixedWidth(140);
  m_searchEdit->setToolTip(tr("Search folders and files by name"));
  applyBrowserSearchPlaceholderStyle(m_searchEdit);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
  if (QGuiApplication *app =
          qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
    connect(app, &QGuiApplication::paletteChanged, m_searchEdit,
            [this](const QPalette &) {
              applyBrowserSearchPlaceholderStyle(m_searchEdit);
            });
  }
#endif
  m_searchAct = addWidget(m_searchEdit);
  m_searchEdit->installEventFilter(this);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &DvItemViewerButtonBar::onSearchTextEdited);

  m_projectFoldersSep = addBlockSep();
  m_projectFolderHost = new QWidget(this);
  m_projectFolderHost->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  auto *projectFolderLay = new QHBoxLayout(m_projectFolderHost);
  projectFolderLay->setContentsMargins(0, 0, 0, 0);
  projectFolderLay->setSpacing(kBrowserIconGap);
  m_projectFolderHostAct = addWidget(m_projectFolderHost);
  m_projectFoldersSep->setVisible(false);
  m_projectFolderHostAct->setVisible(false);

  // Matching left spacer — centers the whole advanced cluster.
  m_rightSpacerAct = addWidget(makeSpacer());

  if (m_itemViewer->m_windowType == DvItemViewer::Browser) {
    m_infoPanelAct = addAction(createQIcon("info"), tr("File Info"));
    m_infoPanelAct->setCheckable(true);
    m_infoPanelAct->setEnabled(true);
    m_infoPanelAct->setToolTip(tr("File Info"));
  }

  uniformAdvancedIconButtons();

  m_advancedDisplayAct = new QAction(tr("Advanced Display"), this);
  m_advancedDisplayAct->setCheckable(true);
  connect(m_advancedDisplayAct, &QAction::toggled, this,
          &DvItemViewerButtonBar::onAdvancedDisplayToggled);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::syncAdvancedControlsFromPanel() {
  if (!m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;

  m_updatingUi = true;
  const int mode = (int)panel->getThumbnailBgMode();
  const bool isAuto = (mode == (int)DvItemViewerPanel::BgAuto);
  if (m_bgWhiteAct)
    m_bgWhiteAct->setChecked(!isAuto && mode == DvItemViewerPanel::BgWhite);
  if (m_bgBlackAct)
    m_bgBlackAct->setChecked(!isAuto && mode == DvItemViewerPanel::BgBlack);
  if (m_bgTransparentAct)
    m_bgTransparentAct->setChecked(!isAuto &&
                                   mode == DvItemViewerPanel::BgTransparent);
  if (m_bgCheckeredAct)
    m_bgCheckeredAct->setChecked(!isAuto &&
                                 mode == DvItemViewerPanel::BgCheckered);
  if (m_sizeSlider) m_sizeSlider->setValue(panel->getIconSize().width());
  updateFpsButtonLabel();
  m_updatingUi = false;
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::persistAdvancedSettings() {
  if (!m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;
  const bool isCast   = m_itemViewer->m_windowType == DvItemViewer::Cast;
  const bool advanced = panel->isAdvancedDisplay();
  if (isCast) {
    CastAdvancedDisplay  = advanced ? 1 : 0;
    CastAdvancedGuiParts = (int)m_guiPartsFlag;
  } else {
    BrowserAdvancedDisplay  = advanced ? 1 : 0;
    BrowserAdvancedGuiParts = (int)m_guiPartsFlag;
  }
  // Keep last advanced size/bg when the option is turned off.
  if (!advanced) return;
  if (isCast) {
    CastThumbnailBg    = (int)panel->getThumbnailBgMode();
    CastThumbnailWidth = panel->getIconSize().width();
  } else {
    BrowserThumbnailBg    = (int)panel->getThumbnailBgMode();
    BrowserThumbnailWidth = panel->getIconSize().width();
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::addGuiShowHideMenu(QMenu *menu) {
  if (!menu) return;
  QMenu *showHideMenu = menu->addMenu(tr("GUI Show / Hide"));

  struct PartDef {
    unsigned int flag;
    const char *label;
  };
  const PartDef parts[] = {
      {AGUI_SizeSlider, QT_TR_NOOP("Size Slider")},
      {AGUI_SizeMenu, QT_TR_NOOP("View Mode")},
      {AGUI_Background, QT_TR_NOOP("Background Modes")},
      {AGUI_TypeFilters, QT_TR_NOOP("Type Filter Icons")},
      {AGUI_TypeFilterList, QT_TR_NOOP("Type Filter List")},
      {AGUI_Favorites, QT_TR_NOOP("Favorites Filter")},
      {AGUI_FavoriteStars, QT_TR_NOOP("Favorite Stars on Thumbnails")},
      {AGUI_PlayFps, QT_TR_NOOP("Playback FPS")},
      {AGUI_Search, QT_TR_NOOP("Search")},
      {AGUI_ProjectFolders, QT_TR_NOOP("Project Folders")},
      {AGUI_InfoPanel, QT_TR_NOOP("Info Panel")},
  };

  QActionGroup *group = new QActionGroup(showHideMenu);
  group->setExclusive(false);
  for (const PartDef &part : parts) {
    QAction *act = showHideMenu->addAction(tr(part.label));
    act->setCheckable(true);
    act->setChecked(m_guiPartsFlag & part.flag);
    act->setData((uint)part.flag);
    group->addAction(act);
  }
  connect(group, &QActionGroup::triggered, this,
          &DvItemViewerButtonBar::onGuiShowHideTriggered);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::updateFpsButtonLabel() {
  if (!m_fpsBtn) return;
  int fps   = 10;
  bool loop = true;
  if (m_itemViewer && m_itemViewer->getPanel()) {
    fps  = m_itemViewer->getPanel()->getPlayFps();
    loop = m_itemViewer->getPanel()->isPlayLoop();
  }
  m_fpsBtn->setToolTip(tr("%1 FPS").arg(fps));
  if (m_fpsField) {
    m_fpsField->blockSignals(true);
    m_fpsField->setValue(fps);
    m_fpsField->blockSignals(false);
  }
  if (m_loopBtn) {
    m_loopBtn->blockSignals(true);
    m_loopBtn->setChecked(loop);
    m_loopBtn->blockSignals(false);
  }
  if (QMenu *menu = m_fpsBtn->menu()) {
    for (QAction *act : menu->actions()) {
      if (!act->data().isValid()) continue;
      act->setChecked(act->data().toInt() == fps);
    }
  }
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  if (!m_advancedDisplayAct) {
    m_advancedDisplayAct = new QAction(tr("Advanced Display"), this);
    m_advancedDisplayAct->setCheckable(true);
    connect(m_advancedDisplayAct, &QAction::toggled, this,
            &DvItemViewerButtonBar::onAdvancedDisplayToggled);
  }

  const bool advancedOn = isAdvancedDisplayOn();
  m_advancedDisplayAct->blockSignals(true);
  m_advancedDisplayAct->setChecked(advancedOn);
  m_advancedDisplayAct->blockSignals(false);
  menu.addAction(m_advancedDisplayAct);

  // Viewer-style submenu — only while Advanced Display is active.
  if (advancedOn) addGuiShowHideMenu(&menu);

  TPanel *tpanel = nullptr;
  for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
    tpanel = dynamic_cast<TPanel *>(w);
    if (tpanel) break;
  }
  if (tpanel) {
    menu.addSeparator();
    tpanel->appendRoomBindMenuAction(&menu);
  }

  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

bool DvItemViewerButtonBar::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_searchEdit && m_searchEdit) {
    const QEvent::Type type = event->type();
    if (type == QEvent::PaletteChange || type == QEvent::StyleChange ||
        type == QEvent::Show)
      applyBrowserSearchPlaceholderStyle(m_searchEdit);
  }
  return QToolBar::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::changeEvent(QEvent *event) {
  QToolBar::changeEvent(event);
  if (!m_searchEdit || !event) return;
  const QEvent::Type type = event->type();
  if (type == QEvent::PaletteChange || type == QEvent::StyleChange)
    applyBrowserSearchPlaceholderStyle(m_searchEdit);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onAdvancedDisplayToggled(bool on) {
  if (!m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;
  panel->setAdvancedDisplay(on);
  refreshAdvancedControlsVisibility();
  syncAdvancedControlsFromPanel();
  persistAdvancedSettings();
  if (!on) {
    m_updatingUi = true;
    for (int i = 0; i < m_typeFilterActs.size(); ++i) {
      m_typeFilterActs[i]->setChecked(false);
      if (i < m_typeFilterMenuActs.size())
        m_typeFilterMenuActs[i]->setChecked(false);
    }
    m_updatingUi = false;
    emit typeFilterChanged(QStringList());
    if (m_searchEdit && !m_searchEdit->text().isEmpty()) m_searchEdit->clear();
  }
  if (DvItemListModel *model = m_itemViewer->getModel()) model->refreshData();
  m_itemViewer->refresh();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onGuiShowHideTriggered(QAction *action) {
  if (!action) return;
  const unsigned int part = action->data().toUInt();
  if (action->isChecked())
    m_guiPartsFlag |= part;
  else
    m_guiPartsFlag &= ~part;
  refreshAdvancedControlsVisibility();
  persistAdvancedSettings();
  if (part == AGUI_FavoriteStars && m_itemViewer && m_itemViewer->getPanel())
    m_itemViewer->getPanel()->update();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onPlayFpsTriggered(QAction *action) {
  if (!action || !m_itemViewer || !action->data().isValid()) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;
  panel->setPlayFps(action->data().toInt());
  updateFpsButtonLabel();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onPlayFpsFieldEdited() {
  if (!m_fpsField || !m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;
  panel->setPlayFps(m_fpsField->getValue());
  updateFpsButtonLabel();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onPlayLoopToggled(bool on) {
  if (!m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;
  panel->setPlayLoop(on);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onBgModeTriggered(QAction *action) {
  if (m_updatingUi || !action || !m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;

  m_updatingUi = true;
  DvItemViewerPanel::ThumbnailBgMode mode = DvItemViewerPanel::BgAuto;
  if (action->isChecked()) {
    mode = (DvItemViewerPanel::ThumbnailBgMode)action->data().toInt();
    if (m_bgGroup) {
      for (QAction *bgAct : m_bgGroup->actions()) {
        if (bgAct != action) bgAct->setChecked(false);
      }
    }
  } else if (m_bgGroup) {
    for (QAction *bgAct : m_bgGroup->actions()) {
      if (bgAct->isChecked()) {
        mode = (DvItemViewerPanel::ThumbnailBgMode)bgAct->data().toInt();
        break;
      }
    }
  }
  m_updatingUi = false;

  panel->setThumbnailBgMode(mode);
  persistAdvancedSettings();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onSizePresetTriggered(QAction *action) {
  if (!action || !m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;
  panel->applyThumbnailSizePreset(
      (DvItemViewerPanel::ThumbnailSizePreset)action->data().toInt());
  syncAdvancedControlsFromPanel();
  persistAdvancedSettings();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onSizeSliderChanged(int value) {
  if (m_updatingUi || !m_itemViewer) return;
  DvItemViewerPanel *panel = m_itemViewer->getPanel();
  if (!panel) return;
  panel->setThumbnailsView();
  panel->setThumbnailWidth(value);
  persistAdvancedSettings();
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onPanelThumbnailSizeChanged(const QSize &size) {
  if (m_updatingUi || !m_sizeSlider) return;
  m_updatingUi = true;
  m_sizeSlider->setValue(size.width());
  m_updatingUi = false;
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onSearchTextEdited(const QString &text) {
  emit searchFilterChanged(text);
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onFavoritesFilterToggled(bool on) {
  emit favoritesFilterChanged(on);
}

//-----------------------------------------------------------------------------

QStringList DvItemViewerButtonBar::selectedTypeExtensions() const {
  QStringList exts;
  for (QAction *act : m_typeFilterActs) {
    if (!act || !act->isChecked()) continue;
    exts.append(act->data().toStringList());
  }
  return exts;
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onTypeFilterTriggered(bool) {
  if (m_updatingUi) return;
  QAction *act = qobject_cast<QAction *>(sender());
  if (!act) return;

  const bool shift =
      (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
  // Qt already toggled this action; recover the prior checked state.
  const bool wasChecked = !act->isChecked();

  m_updatingUi = true;
  if (shift) {
    // Keep the toggled multi-selection as-is.
  } else {
    bool wasSoleSelected = wasChecked;
    for (QAction *other : m_typeFilterActs) {
      if (other != act && other->isChecked()) {
        wasSoleSelected = false;
        break;
      }
    }
    for (QAction *other : m_typeFilterActs) other->setChecked(false);
    // Sole active chip clicked again → clear filter (show all types).
    if (!wasSoleSelected) act->setChecked(true);
  }
  m_updatingUi = false;

  syncTypeFilterMenuFromIcons();
  emit typeFilterChanged(selectedTypeExtensions());
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::syncTypeFilterMenuFromIcons() {
  if (m_updatingUi) return;
  m_updatingUi = true;
  const int n = qMin(m_typeFilterActs.size(), m_typeFilterMenuActs.size());
  for (int i = 0; i < n; ++i)
    m_typeFilterMenuActs[i]->setChecked(m_typeFilterActs[i]->isChecked());
  m_updatingUi = false;
}

//-----------------------------------------------------------------------------

void DvItemViewerButtonBar::onTypeFilterMenuTriggered(bool) {
  if (m_updatingUi) return;
  QAction *menuAct = qobject_cast<QAction *>(sender());
  if (!menuAct) return;
  const int idx = m_typeFilterMenuActs.indexOf(menuAct);
  if (idx < 0 || idx >= m_typeFilterActs.size()) return;

  m_updatingUi = true;
  m_typeFilterActs[idx]->setChecked(menuAct->isChecked());
  m_updatingUi = false;

  emit typeFilterChanged(selectedTypeExtensions());
}
