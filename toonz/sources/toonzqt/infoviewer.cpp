

#include "toonzqt/infoviewer.h"
#include "toonzqt/intfield.h"
#include "tsystem.h"
#include "tlevel.h"
#include "tpalette.h"
#include "tlevel_io.h"
#include "tsound_io.h"
#include "tiio.h"
#include "tstream.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "toonz/toonzscene.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toutputproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/levelset.h"
#include "tcontenthistory.h"
#include "tfiletype.h"
#include <QSlider>
#include <QLabel>
#include <QTextEdit>
#include <QDateTime>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QFontMetrics>
#include <QTimer>

using namespace DVGui;

//----------------------------------------------------------------

class InfoViewerImp {
public:
  enum {
    eFullpath = 0,
    eFileType,
    eFrames,
    eOwner,
    eSize,
    eCreated,
    eModified,
    eLastAccess,
    eImageSize,
    eSaveBox,
    eBitsSample,
    eSamplePixel,
    eDpi,
    eOrientation,
    eCompression,
    eQuality,
    eSmoothing,
    eCodec,
    eAlphaChannel,
    eByteOrdering,
    eHPos,
    ePalettePages,
    ePaletteStyles,
    eCamera,
    eCameraDpi,
    eFrameCount,
    eLevelCount,
    eOutputPath,
    eEndianness,

    // sound info
    eLength,
    eChannels,
    eSampleRate,
    eSampleSize,
    eSampleType,
    eHowMany
  };

  TFilePath m_path;
  TLevelP m_level;
  std::vector<TFrameId> m_fids;
  QStringList m_formats;
  int m_currentIndex;
  int m_frameCount;
  TPalette *m_palette;
  QLabel m_framesLabel;
  IntField m_framesSlider;
  std::vector<std::pair<QLabel *, QLabel *>> m_labels;
  std::vector<QString> m_fullValues;
  QLabel m_historyLabel;
  QTextEdit m_history;
  Separator m_separator1, m_separator2;
  bool m_embeddedStyle = false;
  int m_valueColumnWidth = 0;
  void setFileInfo(const TFileStatus &status);
  void setImageInfo();
  void setSoundInfo();
  // void cleanFileInfo();
  void cleanLevelInfo();
  void setToonzSceneInfo();
  void setPaletteInfo();
  void setGeneralFileInfo(const TFilePath &path);
  QString getTypeString();
  void onSliderChanged();
  TFrameId currentFrameId() const;
  InfoViewerImp();
  ~InfoViewerImp();
  void clear();
  bool setLabel(TPropertyGroup *pg, int index, std::string type);
  void create(int index, QString str);
  void loadPalette(const TFilePath &path);
  void applyEmbeddedStyle(bool embedded);
  void constrainToWidth(int availableWidth);
  void finishDisplay();
  void setHistoryText(const QString &raw);

  static QString manualWrap(const QString &text, const QFontMetrics &fm,
                            int maxWidth) {
    if (maxWidth <= 0 || text.isEmpty()) return text;
    QString result;
    int lineStart = 0;
    int lastBreak = -1;
    for (int i = 0; i < text.length(); ++i) {
      QChar c = text[i];
      if (c == '/' || c == '\\' || c == ' ' || c == ',' || c == ')')
        lastBreak = i + 1;
      int w = fm.horizontalAdvance(text.mid(lineStart, i - lineStart + 1));
      if (w > maxWidth && i > lineStart) {
        int cut = (lastBreak > lineStart) ? lastBreak : i;
        result += text.mid(lineStart, cut - lineStart) + QChar('\n');
        lineStart = cut;
        lastBreak = -1;
      }
    }
    result += text.mid(lineStart);
    return result;
  }

  inline void setVal(int index, const QString &str) {
    if (index < 0 || index >= (int)m_labels.size()) return;
    if ((int)m_fullValues.size() < (int)m_labels.size())
      m_fullValues.resize(m_labels.size());
    m_fullValues[index] = str;
    m_labels[index].second->setToolTip(str);
    if (m_embeddedStyle && m_valueColumnWidth > 0) {
      m_labels[index].second->setText(
          manualWrap(str, m_labels[index].second->fontMetrics(),
                     m_valueColumnWidth));
    } else {
      m_labels[index].second->setText(str);
    }
  }

public slots:

  bool setItem(const TLevelP &level, TPalette *palette, const TFilePath &path);
};

//----------------------------------------------------------------

InfoViewer::InfoViewer(QWidget *parent)
    : Dialog(parent, false, true), m_imp(new InfoViewerImp()), m_embedded(false) {
  setWindowTitle(tr("File Info"));
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

  int i;
  for (i = 0; i < (int)m_imp->m_labels.size(); i++) {
    addWidgets(m_imp->m_labels[i].first, m_imp->m_labels[i].second);
    if (i == InfoViewerImp::eLastAccess) addWidget(&m_imp->m_separator1);
  }

  addWidget(&m_imp->m_separator2);
  addWidget(&m_imp->m_historyLabel);
  addWidget(&m_imp->m_history);

  addWidgets(&m_imp->m_framesLabel, &m_imp->m_framesSlider);

  connect(&m_imp->m_framesSlider, SIGNAL(valueChanged(bool)), this,
          SLOT(onSliderChanged(bool)));
  hide();
}

//----------------------------------------------------------------

void InfoViewer::setEmbedded(bool embedded) {
  if (m_embedded == embedded) return;
  m_embedded = embedded;
  QWidget *parent = parentWidget();
  QLayout *parentLayout = parent ? parent->layout() : nullptr;
  if (embedded) {
    setModal(false);
    setWindowFlags(Qt::Widget);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setTopMargin(4);
    setTopSpacing(2);
    if (QLayout *lay = layout())
      lay->setSizeConstraint(QLayout::SetNoConstraint);
    if (m_mainFrame) {
      m_mainFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      m_mainFrame->setMinimumWidth(0);
      m_mainFrame->setMinimumHeight(0);
    }
    if (m_topLayout) {
      m_topLayout->setSpacing(6);
      m_topLayout->setContentsMargins(4, 4, 4, 4);
      m_topLayout->setAlignment(Qt::AlignTop);
      for (int i = 0; i < m_topLayout->count(); ++i) {
        QBoxLayout *row =
            qobject_cast<QBoxLayout *>(m_topLayout->itemAt(i)->layout());
        if (!row) continue;
        row->setDirection(QBoxLayout::TopToBottom);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);
      }
    }
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumWidth(0);
  } else {
    setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                   Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
    setAlignment(Qt::AlignCenter);
    setTopMargin(12);
    setTopSpacing(5);
    if (QLayout *lay = layout()) lay->setSizeConstraint(QLayout::SetFixedSize);
    if (m_topLayout) {
      for (int i = 0; i < m_topLayout->count(); ++i) {
        QBoxLayout *row =
            qobject_cast<QBoxLayout *>(m_topLayout->itemAt(i)->layout());
        if (!row) continue;
        row->setDirection(QBoxLayout::LeftToRight);
      }
    }
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  }
  m_imp->applyEmbeddedStyle(embedded);
  if (parentLayout && parentLayout->indexOf(this) < 0)
    parentLayout->addWidget(this);
  else if (parent)
    setParent(parent);
}

InfoViewer::~InfoViewer() {}

//----------------------------------------------------------------

QSize InfoViewer::sizeHint() const {
  if (!m_embedded) return Dialog::sizeHint();
  // Keep width narrow so the host QScrollArea owns horizontal sizing.
  int h = m_topLayout ? m_topLayout->sizeHint().height() : 400;
  return QSize(120, h);
}

//----------------------------------------------------------------

QSize InfoViewer::minimumSizeHint() const {
  if (!m_embedded) return Dialog::minimumSizeHint();
  int h = m_topLayout ? m_topLayout->minimumSize().height() : 80;
  return QSize(80, h);
}

//----------------------------------------------------------------

void InfoViewer::resizeEvent(QResizeEvent *event) {
  if (m_embedded) {
    QWidget::resizeEvent(event);
    int margins = 0;
    if (m_topLayout) {
      int l, r;
      m_topLayout->getContentsMargins(&l, nullptr, &r, nullptr);
      margins = l + r;
    }
    m_imp->constrainToWidth(qMax(80, width() - margins - 12));
    return;
  }
  Dialog::resizeEvent(event);
}

//----------------------------------------------------------------

void InfoViewer::hideEvent(QHideEvent *event) {
  // Avoid Dialog::hideEvent geometry restore while embedded as a side panel.
  if (m_embedded) {
    QWidget::hideEvent(event);
    return;
  }
  m_imp->m_level = TLevelP();
  Dialog::hideEvent(event);
}

//----------------------------------------------------------------
void InfoViewer::onSliderChanged(bool) {
  m_imp->onSliderChanged();
  if (m_embedded) emit currentFrameChanged();
}

//----------------------------------------------------------------

TFrameId InfoViewer::currentFrameId() const { return m_imp->currentFrameId(); }

//----------------------------------------------------------------

TFrameId InfoViewerImp::currentFrameId() const {
  if (m_fids.empty() || m_currentIndex < 0 ||
      m_currentIndex >= (int)m_fids.size())
    return TFrameId::NO_FRAME;
  return m_fids[m_currentIndex];
}

//----------------------------------------------------------------

void InfoViewerImp::onSliderChanged() {
  if (m_framesSlider.getValue() - 1 == m_currentIndex) return;

  m_currentIndex = m_framesSlider.getValue() - 1;
  setImageInfo();
}

//----------------------------------------------------------------

namespace {
void setLabelStyle(QLabel *l) { l->setObjectName("TitleTxtLabel"); }
}  // namespace

//----------------------------------------------------------------

void InfoViewerImp::create(int index, QString str) {
  m_labels[index] =
      std::pair<QLabel *, QLabel *>(new QLabel(str), new QLabel(""));
  setLabelStyle(m_labels[index].first);
}

//----------------------------------------------------------------

InfoViewerImp::InfoViewerImp()
    : m_palette(0)
    , m_framesLabel(QObject::tr("Current Frame: "))
    , m_framesSlider()
    , m_history()
    , m_historyLabel(QObject::tr("File History")) {
  setLabelStyle(&m_framesLabel);

  TLevelReader::getSupportedFormats(m_formats);
  TSoundTrackReader::getSupportedFormats(m_formats);

  m_labels.resize(eHowMany);
  m_fullValues.resize(eHowMany);

  create(eFullpath, QObject::tr("Fullpath:     "));
  create(eFileType, QObject::tr("File Type:    "));
  create(eFrames, QObject::tr("Frames:       "));
  create(eOwner, QObject::tr("Owner:        "));
  create(eSize, QObject::tr("Size:         "));

  create(eCreated, QObject::tr("Created:      "));
  create(eModified, QObject::tr("Modified:     "));
  create(eLastAccess, QObject::tr("Last Access:  "));

  // level info

  create(eImageSize, QObject::tr("Image Size:   "));
  create(eSaveBox, QObject::tr("SaveBox:      "));
  create(eBitsSample, QObject::tr("Bits/Sample:  "));
  create(eSamplePixel, QObject::tr("Sample/Pixel: "));
  create(eDpi, QObject::tr("Dpi:          "));
  create(eOrientation, QObject::tr("Orientation:  "));
  create(eCompression, QObject::tr("Compression:  "));
  create(eQuality, QObject::tr("Quality:      "));
  create(eSmoothing, QObject::tr("Smoothing:    "));
  create(eCodec, QObject::tr("Codec:        "));
  create(eAlphaChannel, QObject::tr("Alpha Channel:"));
  create(eByteOrdering, QObject::tr("Byte Ordering:"));
  create(eHPos, QObject::tr("H Pos:"));
  create(ePalettePages, QObject::tr("Palette Pages:"));
  create(ePaletteStyles, QObject::tr("Palette Styles:"));

  create(eCamera, QObject::tr("Camera Size:      "));
  create(eCameraDpi, QObject::tr("Camera Dpi:       "));
  create(eFrameCount, QObject::tr("Number of Frames: "));
  create(eLevelCount, QObject::tr("Number of Levels: "));
  create(eOutputPath, QObject::tr("Output Path:      "));
  create(eEndianness, QObject::tr("Endianness:      "));

  // sound info
  create(eLength, QObject::tr("Length:       "));
  create(eChannels, QObject::tr("Channels: "));
  create(eSampleRate, QObject::tr("Sample Rate: "));
  create(eSampleSize, QObject::tr("Sample Size:      "));
  create(eSampleType, QObject::tr("Sample Type: "));

  m_historyLabel.setStyleSheet("color: rgb(0, 0, 200);");
  m_history.setStyleSheet("font-size: 12px; font-family: \"courier\";");
  // m_history.setStyleSheet("font-family: \"courier\";");
  m_history.setReadOnly(true);
  m_history.setFixedWidth(490);
}

//----------------------------------------------------------------

void InfoViewerImp::clear() {
  int i;

  for (i = 0; i < (int)m_labels.size(); i++) setVal(i, "");

  m_history.clear();
}

//----------------------------------------------------------------

void InfoViewerImp::applyEmbeddedStyle(bool embedded) {
  m_embeddedStyle = embedded;

  if (embedded) {
    m_history.setMinimumWidth(0);
    m_history.setMaximumWidth(QWIDGETSIZE_MAX);
    m_history.setMinimumHeight(0);
    m_history.setMaximumHeight(QWIDGETSIZE_MAX);
    m_history.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_history.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_history.setLineWrapMode(QTextEdit::WidgetWidth);
    m_history.setStyleSheet("font-size: 12px;");
    m_history.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_historyLabel.setWordWrap(true);
    m_historyLabel.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  } else {
    m_history.setFixedWidth(490);
    m_history.setMinimumHeight(0);
    m_history.setMaximumHeight(QWIDGETSIZE_MAX);
    m_history.setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_history.setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_history.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_historyLabel.setMaximumWidth(QWIDGETSIZE_MAX);
    m_valueColumnWidth = 0;
  }

  for (auto &pair : m_labels) {
    if (!pair.first || !pair.second) continue;
    pair.first->setWordWrap(false);
    pair.second->setWordWrap(false);
    if (embedded) {
      pair.first->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
      pair.second->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
      pair.second->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    } else {
      pair.first->setMaximumWidth(QWIDGETSIZE_MAX);
      pair.second->setMaximumWidth(QWIDGETSIZE_MAX);
      pair.first->setMinimumHeight(0);
      pair.second->setMinimumHeight(0);
      pair.first->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
      pair.second->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
  }

  if (embedded) {
    m_framesLabel.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_framesSlider.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_framesSlider.setMinimumHeight(24);
  } else {
    m_framesLabel.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_framesSlider.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  }
}

//----------------------------------------------------------------

void InfoViewerImp::constrainToWidth(int availableWidth) {
  if (availableWidth < 80) availableWidth = 80;
  m_valueColumnWidth = availableWidth;

  if ((int)m_fullValues.size() < (int)m_labels.size())
    m_fullValues.resize(m_labels.size());
  for (int i = 0; i < (int)m_labels.size(); ++i) {
    if (m_labels[i].first) m_labels[i].first->setMaximumWidth(availableWidth);
    if (!m_labels[i].second) continue;
    m_labels[i].second->setMaximumWidth(availableWidth);
    const QString &full = m_fullValues[i];
    if (!full.isEmpty()) {
      m_labels[i].second->setText(
          manualWrap(full, m_labels[i].second->fontMetrics(), availableWidth));
    }
  }
  m_framesLabel.setMaximumWidth(availableWidth);
  m_framesSlider.setMaximumWidth(availableWidth);
  m_historyLabel.setMaximumWidth(availableWidth);
  m_history.setMaximumWidth(availableWidth);
  m_separator1.setMaximumWidth(availableWidth);
  m_separator2.setMaximumWidth(availableWidth);

  if (m_embeddedStyle && !m_history.toPlainText().isEmpty()) {
    QTextDocument *doc = m_history.document();
    doc->setTextWidth(availableWidth - 4);
    int h = (int)doc->size().height() + 6;
    m_history.setFixedHeight(qMax(30, h));
  }
}

//----------------------------------------------------------------

void InfoViewerImp::setHistoryText(const QString &raw) {
  // serialize() uses fixed-width columns bordered by '|', records by "||".
  // Embedded mode reformats each record as stacked label/value lines.
  QString cleaned = raw;
  cleaned.remove('\n');
  cleaned.remove(QChar(0));

  if (!m_embeddedStyle) {
    QString str = cleaned;
    str         = str.replace("||", "\n");
    str         = str.remove('|');
    m_history.setPlainText(str);
    return;
  }

  QStringList records = cleaned.split("||", Qt::SkipEmptyParts);
  if (records.isEmpty()) {
    m_history.clear();
    return;
  }

  for (QString &rec : records) {
    if (rec.startsWith('|')) rec = rec.mid(1);
    if (rec.endsWith('|')) rec.chop(1);
  }

  const QString &header = records[0];
  int dateCol    = header.indexOf("DATE:");
  int machCol    = header.indexOf("MACHINE:");
  int userCol    = header.indexOf("USER:");
  int framesCol  = header.indexOf("FRAMES");
  bool hasFrames = (framesCol >= 0);

  QString result;
  for (int r = 1; r < records.size(); ++r) {
    const QString &line = records[r];
    if (r > 1) result += "\n";

    if (dateCol > 0 && dateCol <= line.length()) {
      QString num = line.left(dateCol).trimmed();
      if (!num.isEmpty()) result += num;
    }

    if (dateCol >= 0 && machCol > dateCol && machCol <= line.length()) {
      QString datetime = line.mid(dateCol, machCol - dateCol).trimmed();
      // Date and time are separated by three spaces in the serialized format.
      int splitPos = datetime.indexOf(QStringLiteral("   "));
      if (splitPos > 0) {
        QString datePart = datetime.left(splitPos).trimmed();
        QString timePart = datetime.mid(splitPos).trimmed();
        if (!datePart.isEmpty()) result += "\n  Date: " + datePart;
        if (!timePart.isEmpty()) result += "\n  Time: " + timePart;
      } else if (!datetime.isEmpty()) {
        result += "\n  Date: " + datetime;
      }
    }

    if (machCol >= 0 && userCol > machCol && userCol <= line.length()) {
      QString machine = line.mid(machCol, userCol - machCol).trimmed();
      if (!machine.isEmpty()) result += "\n  Machine: " + machine;
    }

    int userEnd = hasFrames ? framesCol : line.length();
    if (userCol >= 0 && userEnd > userCol && userCol < line.length()) {
      QString user = line.mid(userCol, userEnd - userCol).trimmed();
      if (!user.isEmpty()) result += "\n  User: " + user;
    }

    if (hasFrames && framesCol < line.length()) {
      QString frames = line.mid(framesCol).trimmed();
      if (!frames.isEmpty()) result += "\n  Frames: " + frames;
    }
  }

  m_history.setPlainText(result);
}

//----------------------------------------------------------------

void InfoViewerImp::finishDisplay() {
  if ((int)m_fullValues.size() < (int)m_labels.size())
    m_fullValues.resize(m_labels.size());

  for (int i = 0; i < (int)m_labels.size(); i++) {
    const bool empty = m_fullValues[i].isEmpty();
    if (empty)
      m_labels[i].first->hide(), m_labels[i].second->hide();
    else
      m_labels[i].first->show(), m_labels[i].second->show();
  }

  if (m_history.toPlainText() == "") {
    m_separator2.hide();
    m_historyLabel.hide();
    m_history.hide();
  } else {
    m_separator2.show();
    m_historyLabel.show();
    m_history.show();
  }
}

//----------------------------------------------------------------

InfoViewerImp::~InfoViewerImp() {
  int i;
  for (i = 0; i < (int)m_labels.size(); i++) {
    delete m_labels[i].first;
    delete m_labels[i].second;
  }

  m_labels.clear();
}

//----------------------------------------------------------------

void InfoViewerImp::setFileInfo(const TFileStatus &status) {
  // m_fPath.setText(status.
}

//----------------------------------------------------------------

QString InfoViewerImp::getTypeString() {
  QString ext = QString::fromStdString(m_path.getType());

  if (ext == "tlv" || ext == "tzp" || ext == "tzu")
    return "Toonz Cmapped Raster Level";
  else if (ext == "pli" || ext == "svg")
    return "Toonz Vector Level";
  else if (ext == "mov" || ext == "avi" || ext == "3gp")
    return "Movie File";
  else if (ext == "tnz")
    return "Toonz Scene";
  else if (ext == "tab")
    return "Tab Scene";
  else if (ext == "plt")
    return "Toonz Palette";
  else if (ext == "wav" || ext == "aiff" || ext == "aif" || ext == "raw" ||
           ext == "mp3" || ext == "ogg" || ext == "flac")
    return "Audio File";
  else if (ext == "mesh")
    return "Toonz Mesh Level";
  else if (ext == "tzm")
    return "Toonz Meta Level";
  else if (ext == "pic")
    return "Pic File";
  else if (Tiio::makeReader(ext.toStdString()))
    return (m_fids.size() == 1) ? "Single Raster Image" : "Raster Image Level";
  else if (ext == "psd")
    return "Photoshop Image";
  else
    return "Unmanaged File Type";
}

//----------------------------------------------------------------

void InfoViewerImp::setGeneralFileInfo(const TFilePath &path) {
  QFileInfo fi = toQString(path);
  assert(fi.exists());

  setVal(eFullpath, fi.absoluteFilePath());
  setVal(eFileType, getTypeString());
  if (fi.owner() != "") setVal(eOwner, fi.owner());
  setVal(eSize, fileSizeString(fi.size()));
  setVal(eCreated, fi.birthTime().toString());
  setVal(eModified, fi.lastModified().toString());
  setVal(eLastAccess, fi.lastRead().toString());
  m_separator1.show();
}

//----------------------------------------------------------------

bool InfoViewerImp::setLabel(TPropertyGroup *pg, int index, std::string type) {
  TProperty *p = pg->getProperty(type);
  if (!p) return false;
  QString str = QString::fromStdString(p->getValueAsString());
  if (dynamic_cast<TBoolProperty *>(p))
    setVal(index, str == "0" ? "No" : "Yes");
  else
    setVal(index, str);
  return true;
}

//----------------------------------------------------------------

void InfoViewerImp::setImageInfo() {
  if (m_fids.empty() || m_currentIndex < 0 ||
      m_currentIndex >= (int)m_fids.size())
    return;

  if (m_path != TFilePath())
    setGeneralFileInfo(m_path.getType() == "tlv" || !m_path.isLevelName()
                           ? m_path
                           : m_path.withFrame(m_fids[m_currentIndex]));

  assert(m_level);

  setVal(eFrames, QString::number(m_level->getFrameCount()));

  TLevelReaderP lr(m_path);
  const TImageInfo *ii;
  try {
    ii = lr->getImageInfo(m_fids[m_currentIndex]);
  } catch (...) {
    return;
  }
  if (!m_fids.empty() && lr && ii) {
    setVal(eImageSize,
           QString::number(ii->m_lx) + " X " + QString::number(ii->m_ly));
    if (ii->m_x0 <= ii->m_x1)
      setVal(eSaveBox, "(" + QString::number(ii->m_x0) + ", " +
                           QString::number(ii->m_y0) + ", " +
                           QString::number(ii->m_x1) + ", " +
                           QString::number(ii->m_y1) + ")");
    if (ii->m_bitsPerSample > 0)
      setVal(eBitsSample, QString::number(ii->m_bitsPerSample));
    if (ii->m_samplePerPixel > 0)
      setVal(eSamplePixel, QString::number(ii->m_samplePerPixel));
    if (ii->m_dpix > 0 || ii->m_dpiy > 0)
      setVal(eDpi, "(" + QString::number(ii->m_dpix) + ", " +
                       QString::number(ii->m_dpiy) + ")");
    TPropertyGroup *pg = ii->m_properties;
    if (pg) {
      setLabel(pg, eOrientation, "Orientation");
      if (!setLabel(pg, eCompression, "Compression") &&
          !setLabel(pg, eCompression, "Compression Type") &&
          !setLabel(pg, eCompression, "RLE-Compressed"))
        setLabel(pg, eCompression, "File Compression");
      setLabel(pg, eQuality, "Quality");
      setLabel(pg, eSmoothing, "Smoothing");
      setLabel(pg, eCodec, "Codec");
      setLabel(pg, eAlphaChannel, "Alpha Channel");
      setLabel(pg, eByteOrdering, "Byte Ordering");
      setLabel(pg, eEndianness, "Endianness");
    }
  } else
    m_separator1.hide();

  const TContentHistory *ch = 0;
  if (lr) ch = lr->getContentHistory();

  if (ch) {
    setHistoryText(ch->serialize());
  }

  TImageP img        = m_level->frame(m_fids[m_currentIndex]);
  TToonzImageP timg  = (TToonzImageP)img;
  TRasterImageP rimg = (TRasterImageP)img;
  TVectorImageP vimg = (TVectorImageP)img;

  if (img) {
    TRect r = convert(timg->getBBox());
    if (r.x0 <= r.x1)
      setVal(eSaveBox,
             "(" + QString::number(r.x0) + ", " + QString::number(r.y0) + ", " +
                 QString::number(r.x1) + ", " + QString::number(r.y1) + ")");
  }

  double dpix, dpiy;

  if (timg) {
    // setVal(eHPos, QString::number(timg->gethPos()));
    timg->getDpi(dpix, dpiy);
    setVal(eDpi,
           "(" + QString::number(dpix) + ", " + QString::number(dpiy) + ")");
    TDimension dim = timg->getRaster()->getSize();
    setVal(eImageSize,
           QString::number(dim.lx) + " X " + QString::number(dim.ly));
    m_palette = timg->getPalette();
  } else if (rimg) {
    rimg->getDpi(dpix, dpiy);
    setVal(eDpi,
           "(" + QString::number(dpix) + ", " + QString::number(dpiy) + ")");
    TDimension dim = rimg->getRaster()->getSize();
    setVal(eImageSize,
           QString::number(dim.lx) + " X " + QString::number(dim.ly));
  } else if (vimg)
    m_palette = vimg->getPalette();

  // TImageP img = m_level->frame(m_fids[m_currentIndex]);
}

//----------------------------------------------------------------

void InfoViewerImp::setSoundInfo() {
  if (m_path != TFilePath()) setGeneralFileInfo(m_path);
  TSoundTrackP sndTrack;
  try {
    TSoundTrackReaderP sr(m_path);
    if (sr) sndTrack = sr->load();
  } catch (...) {
    return;
  }
  if (!sndTrack) return;

  int seconds = sndTrack->getDuration();
  int minutes = seconds / 60;
  seconds     = seconds % 60;
  QString label;
  if (minutes > 0) label += QString::number(minutes) + " min ";
  label += QString::number(seconds) + " sec";
  setVal(eLength, label);

  label = QString::number(sndTrack->getChannelCount());
  setVal(eChannels, label);

  TUINT32 sampleRate = sndTrack->getSampleRate();
  label              = QString::number(sampleRate / 1000) + " KHz";
  setVal(eSampleRate, label);

  label = QString::number(sndTrack->getBitPerSample()) + " bit";
  setVal(eSampleSize, label);

  switch (sndTrack->getSampleType()) {
  case TSound::INT:
    label = "Signed integer";
    break;
  case TSound::UINT:
    label = "Unsigned integer";
    break;
  case TSound::FLOAT:
    label = "Floating-point";
    break;
  default:
    label = "Unknown";
    break;
  }
  setVal(eSampleType, label);
}

//----------------------------------------------------------------

void InfoViewerImp::cleanLevelInfo() {}

//----------------------------------------------------------------

void InfoViewer::setItem(const TLevelP &level, TPalette *palette,
                         const TFilePath &path) {
  // Prefer setVisible: QDialog::show() is not virtual and can float an
  // embedded viewer.
  if (m_imp->setItem(level, palette, path)) {
    setVisible(true);
    if (m_embedded) {
      int margins = 0;
      if (m_topLayout) {
        int l, r;
        m_topLayout->getContentsMargins(&l, nullptr, &r, nullptr);
        margins = l + r;
      }
      m_imp->constrainToWidth(qMax(80, width() - margins - 12));
      updateGeometry();
      if (m_mainFrame) m_mainFrame->updateGeometry();
      if (QWidget *host = parentWidget()) host->updateGeometry();
      // Re-apply width after the host layout settles.
      QTimer::singleShot(0, this, [this]() {
        if (!m_embedded) return;
        int m = 0;
        if (m_topLayout) {
          int l, r;
          m_topLayout->getContentsMargins(&l, nullptr, &r, nullptr);
          m = l + r;
        }
        m_imp->constrainToWidth(qMax(80, width() - m - 12));
        updateGeometry();
      });
    }
  } else if (!m_embedded) {
    setVisible(false);
  }
}

//----------------------------------------------------------------

void InfoViewerImp::setToonzSceneInfo() {
  ToonzScene scene;
  try {
    scene.loadNoResources(m_path);
  } catch (...) {
    return;
  }

  TCamera *cam = scene.getCurrentCamera();
  if (!cam) return;

  TContentHistory *ch = scene.getContentHistory();
  if (ch) {
    setHistoryText(ch->serialize());
  }

  TLevelSet *set           = scene.getLevelSet();
  TSceneProperties *prop   = scene.getProperties();
  TOutputProperties *oprop = prop->getOutputProperties();

  setVal(eCamera, QString::number(cam->getRes().lx) + " X " +
                      QString::number(cam->getRes().ly));
  setVal(eCameraDpi, QString::number(cam->getDpi().x) + ", " +
                         QString::number(cam->getDpi().y));
  setVal(eFrameCount, QString::number(scene.getFrameCount()));
  if (set) setVal(eLevelCount, QString::number(set->getLevelCount()));

  if (oprop) setVal(eOutputPath, toQString(oprop->getPath()));
}

//----------------------------------------------------------------

void InfoViewerImp::setPaletteInfo() {
  if (!m_palette) return;

  setVal(ePalettePages, QString::number(m_palette->getPageCount()));
  setVal(ePaletteStyles, QString::number(m_palette->getStyleCount()));
}

//----------------------------------------------------------------

void InfoViewerImp::loadPalette(const TFilePath &path) {
  TIStream is(path);
  if (is) {
    TPersist *p = 0;
    is >> p;
    m_palette = dynamic_cast<TPalette *>(p);
  }
}

//----------------------------------------------------------------

bool InfoViewerImp::setItem(const TLevelP &level, TPalette *palette,
                            const TFilePath &path) {
  int i;
  clear();

  m_path  = path;
  m_level = level;
  m_fids.clear();
  m_currentIndex = 0;
  m_palette      = palette;
  m_framesLabel.hide();
  m_framesSlider.hide();
  m_separator1.hide();
  m_separator2.hide();

  m_formats.clear();
  TLevelReader::getSupportedFormats(m_formats);
  TSoundTrackReader::getSupportedFormats(m_formats);

  QString ext = QString::fromStdString(m_path.getType());

  if (m_path != TFilePath() && !m_formats.contains(ext) &&
      !Tiio::makeReader(m_path.getType())) {
    // Non-image file (plt, tnz, ...)
    assert(!m_level);

    if (!TSystem::doesExistFileOrLevel(m_path)) {
      DVGui::warning(QObject::tr("The file %1 does not exist.")
                         .arg(QString::fromStdWString(path.getWideString())));

      return false;
    }

    setGeneralFileInfo(m_path);

    if (ext == "plt") {
      assert(!m_level && !m_palette);
      loadPalette(m_path);
    } else if (ext == "tnz")
      setToonzSceneInfo();
  } else if (TFileType::getInfo(m_path) == TFileType::AUDIO_LEVEL) {
    setSoundInfo();
  } else {
    if (ext == "tlv") loadPalette(m_path.withNoFrame().withType("tpl"));

    if (!m_level) {
      assert(m_path != TFilePath());
      TLevelReaderP lr;
      // Retry with the literal path if TFilePath stripped a frame from the name.
      try {
        lr = TLevelReaderP(m_path);
      } catch (...) {
        lr = TLevelReaderP();
      }
      if (!lr) {
        try {
          TFilePath literalPath(toQString(path).toStdWString());
          lr = TLevelReaderP(literalPath);
        } catch (...) {
          lr = TLevelReaderP();
        }
      }
      if (lr) {
        try {
          m_level = lr->loadInfo();
        } catch (...) {
          m_level = TLevelP();
        }
      }
    }

    if (m_level && m_level->getFrameCount() > 0) {
      bool isMovieFile =
          (ext != "tlv" && m_formats.contains(ext) && !m_path.isLevelName());

      m_frameCount = m_level->getFrameCount();
      m_fids.resize(m_frameCount);
      TLevel::Iterator it = m_level->begin();
      for (i = 0; it != m_level->end() && i < m_frameCount; ++it, ++i)
        m_fids[i] = it->first;

      if (m_frameCount > 1 && !isMovieFile) {
        m_framesSlider.setRange(1, m_frameCount);
        m_framesSlider.setValue(0);
        m_framesSlider.show();
        m_framesLabel.show();
      }

      setImageInfo();
    } else {
      setGeneralFileInfo(m_path);
      TLevelReaderP lr2;
      try {
        lr2 = TLevelReaderP(m_path);
      } catch (...) {
      }
      if (lr2) {
        try {
          TLevelP lvl = lr2->loadInfo();
          if (lvl && lvl->getFrameCount() > 0) {
            setVal(eFrames, QString::number(lvl->getFrameCount()));
            m_level      = lvl;
            m_frameCount = lvl->getFrameCount();
            m_fids.resize(m_frameCount);
            TLevel::Iterator it2 = lvl->begin();
            for (int j = 0; it2 != lvl->end() && j < m_frameCount; ++it2, ++j)
              m_fids[j] = it2->first;
            if (m_frameCount > 1) {
              m_framesSlider.setRange(1, m_frameCount);
              m_framesSlider.setValue(0);
              m_framesSlider.show();
              m_framesLabel.show();
            }
          }
        } catch (...) {
        }
        try {
          const TImageInfo *ii = lr2->getImageInfo(TFrameId(1));
          if (!ii) ii = lr2->getImageInfo(TFrameId::NO_FRAME);
          if (ii) {
            setVal(eImageSize, QString::number(ii->m_lx) + " X " +
                                   QString::number(ii->m_ly));
            if (ii->m_x0 <= ii->m_x1)
              setVal(eSaveBox,
                     "(" + QString::number(ii->m_x0) + ", " +
                         QString::number(ii->m_y0) + ", " +
                         QString::number(ii->m_x1) + ", " +
                         QString::number(ii->m_y1) + ")");
            if (ii->m_bitsPerSample > 0)
              setVal(eBitsSample, QString::number(ii->m_bitsPerSample));
            if (ii->m_samplePerPixel > 0)
              setVal(eSamplePixel, QString::number(ii->m_samplePerPixel));
            if (ii->m_dpix > 0 || ii->m_dpiy > 0)
              setVal(eDpi, "(" + QString::number(ii->m_dpix) + ", " +
                               QString::number(ii->m_dpiy) + ")");
            TPropertyGroup *pg = ii->m_properties;
            if (pg) {
              setLabel(pg, eOrientation, "Orientation");
              if (!setLabel(pg, eCompression, "Compression") &&
                  !setLabel(pg, eCompression, "Compression Type") &&
                  !setLabel(pg, eCompression, "RLE-Compressed"))
                setLabel(pg, eCompression, "File Compression");
              setLabel(pg, eQuality, "Quality");
              setLabel(pg, eSmoothing, "Smoothing");
              setLabel(pg, eCodec, "Codec");
              setLabel(pg, eAlphaChannel, "Alpha Channel");
              setLabel(pg, eByteOrdering, "Byte Ordering");
              setLabel(pg, eEndianness, "Endianness");
            }
          }
        } catch (...) {
        }
      }
    }
  }

  if (m_palette) setPaletteInfo();

  finishDisplay();
  return true;
}
