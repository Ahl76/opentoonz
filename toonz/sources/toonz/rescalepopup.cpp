

#include "rescalepopup.h"

#include "menubarcommandids.h"
#include "tapp.h"
#include "filebrowser.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/lineedit.h"

#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzfolders.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "tfiletype.h"
#include "tlevel_io.h"
#include "tenv.h"

#include "tsystem.h"
#include "trop.h"
#include "tcommon.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QFont>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <cmath>

namespace {

const int kFieldWidth    = 56;
const int kSectionSpace  = 14;
const int kSectionIndent = 16;

TEnv::IntVar RescalePopupSkipExisting("RescalePopupSkipExisting", 1);
TEnv::IntVar RescalePopupRemoveDot("RescalePopupRemoveDot", 1);
TEnv::IntVar RescalePopupPreserveAspectRatio("RescalePopupPreserveAspectRatio",
                                             1);

const QString kCustomPreset = QObject::tr("<custom>");

TRop::ResampleFilterType filterFromIndex(int index) {
  switch (index) {
  case 1:
    return TRop::Mitchell;
  case 2:
    return TRop::Lanczos3;
  default:
    return TRop::Triangle;
  }
}

bool parsePresetResolution(const QString &line, int &xres, int &yres) {
  static const QRegularExpression rx(R"(([0-9]+)\s*x\s*([0-9]+))",
                                     QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch match = rx.match(line);
  if (!match.hasMatch()) return false;
  xres = match.captured(1).toInt();
  yres = match.captured(2).toInt();
  return xres > 0 && yres > 0;
}

QString aspectRatioString(double ar) {
  if (ar <= 0.0) return "1/1";
  const int maxDenom = 64;
  int bestNum = 1, bestDen = 1;
  double bestErr = 1.0;
  for (int den = 1; den <= maxDenom; ++den) {
    int num = (int)(ar * den + 0.5);
    if (num <= 0) continue;
    double err = std::abs(ar - (double)num / den);
    if (err < bestErr) {
      bestErr = err;
      bestNum = num;
      bestDen = den;
    }
  }
  return QString("%1/%2").arg(bestNum).arg(bestDen);
}

}  // namespace

//=============================================================================
// RescalePopup::Worker
//-----------------------------------------------------------------------------

class RescalePopup::Worker final : public QThread {
  RescalePopup *m_popup;

public:
  explicit Worker(RescalePopup *popup) : m_popup(popup) {}

  void run() override {
    RescalePopup *popup     = m_popup;
    const TDimension target = popup->targetSize();
    const TRop::ResampleFilterType filter =
        filterFromIndex(popup->m_filterCombo->currentIndex());
    const bool removeDot = popup->m_removeDotBeforeFrameNumber->isChecked();
    const bool preserveAspectRatio = popup->m_preserveAspectRatio->isChecked();
    int skipped                    = 0;

    for (int i = 0;
         !popup->m_notifier->abortTask() && i < (int)popup->m_srcFiles.size();
         ++i) {
      const TFilePath source  = popup->m_srcFiles[i];
      const TFilePath dest    = popup->destinationPath(source);
      const QString levelName = QString::fromStdWString(source.getLevelNameW());

      if (TSystem::doesExistFileOrLevel(dest)) {
        if (popup->m_skipExisting->isChecked()) {
          skipped++;
          continue;
        }
        TSystem::removeFileOrLevel(dest);
      }

      popup->m_progressDialog->setLabelText(
          popup->m_srcFiles.size() == 1
              ? RescalePopup::tr("Rescaling %1").arg(levelName)
              : RescalePopup::tr("Rescaling %1 of %2: %3")
                    .arg(i + 1)
                    .arg(popup->m_srcFiles.size())
                    .arg(levelName));

      popup->m_progressDialog->setMinimum(0);
      popup->m_progressDialog->setMaximum(100);
      popup->m_progressDialog->setValue(0);
      popup->m_notifier->notifyFrameCompleted(0);

      TFrameId from, to;
      popup->getFrameRange(source, from, to);
      ImageUtils::rescale(source, dest, target, filter, popup->m_notifier, from,
                          to, removeDot, preserveAspectRatio);
    }

    if (skipped > 0) {
      DVGui::info(
          RescalePopup::tr("%1 file(s) skipped (already exist).").arg(skipped));
    }
  }
};

//=============================================================================
// RescalePopup
//-----------------------------------------------------------------------------

RescalePopup::RescalePopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false, "Rescale")
    , m_currentWidthFld(nullptr)
    , m_currentHeightFld(nullptr)
    , m_preserveAspectRatio(nullptr)
    , m_saveInFld(nullptr)
    , m_widthFld(nullptr)
    , m_heightFld(nullptr)
    , m_fileNameFld(nullptr)
    , m_fromFld(nullptr)
    , m_toFld(nullptr)
    , m_skipExisting(nullptr)
    , m_removeDotBeforeFrameNumber(nullptr)
    , m_presetCombo(nullptr)
    , m_filterCombo(nullptr)
    , m_addPresetBtn(nullptr)
    , m_removePresetBtn(nullptr)
    , m_okBtn(nullptr)
    , m_cancelBtn(nullptr)
    , m_progressDialog(nullptr)
    , m_notifier(nullptr)
    , m_worker(nullptr)
    , m_currentSize()
    , m_isRunning(false)
    , m_updatingPreset(false)
    , m_updatingSize(false) {
  setModal(false);
  setWindowTitle(tr("Rescale"));
  setMinimumWidth(420);

  m_presetListFile = ToonzFolder::getReslistPath(false).getQString();

  m_topLayout->setContentsMargins(8, 8, 8, 8);
  m_topLayout->setSpacing(0);

  QGridLayout *grid = new QGridLayout();
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(4);
  grid->setVerticalSpacing(5);
  grid->setColumnStretch(0, 0);
  grid->setColumnStretch(1, 0);
  grid->setColumnStretch(2, 1);
  grid->setColumnMinimumWidth(0, kSectionIndent);

  auto sectionGap = [&](int &row) {
    if (row > 0) grid->setRowMinimumHeight(row++, kSectionSpace);
  };

  auto sectionTitle = [&](int &row, const QString &title) {
    sectionGap(row);
    QLabel *hdr = new QLabel(title, this);
    QFont f     = hdr->font();
    f.setBold(true);
    hdr->setFont(f);
    grid->addWidget(hdr, row++, 0, 1, 3, Qt::AlignLeft);
  };

  auto fieldLabel = [&](const QString &text) {
    QLabel *lbl = new QLabel(text, this);
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return lbl;
  };

  auto resolutionWidget = [&](DVGui::IntLineEdit *w, DVGui::IntLineEdit *h) {
    w->setFixedSize(kFieldWidth, DVGui::WidgetHeight);
    h->setFixedSize(kFieldWidth, DVGui::WidgetHeight);
    QHBoxLayout *lay = new QHBoxLayout();
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(w);
    lay->addWidget(new QLabel(tr("×"), this));
    lay->addWidget(h);
    lay->addStretch();
    QWidget *box = new QWidget(this);
    box->setLayout(lay);
    return box;
  };

  auto sizeRowWidget = [&](DVGui::IntLineEdit *w, DVGui::IntLineEdit *h,
                           QWidget *trailing = nullptr) {
    QHBoxLayout *lay = new QHBoxLayout();
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);
    lay->addWidget(resolutionWidget(w, h));
    if (trailing) lay->addWidget(trailing);
    lay->addStretch();
    QWidget *box = new QWidget(this);
    box->setLayout(lay);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return box;
  };

  auto presetWidget = [&](QComboBox *combo, bool withButtons) {
    combo->setFixedHeight(DVGui::WidgetHeight);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout *lay = new QHBoxLayout();
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(3);
    lay->addWidget(combo, 1);
    if (withButtons) {
      lay->addWidget(m_addPresetBtn, 0);
      lay->addWidget(m_removePresetBtn, 0);
    }
    QWidget *box = new QWidget(this);
    box->setLayout(lay);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return box;
  };

  int row = 0;

  // --- Current Size (read-only) ---
  sectionTitle(row, tr("Current Size"));

  m_currentWidthFld  = new DVGui::IntLineEdit(this, 0, 0, 16384);
  m_currentHeightFld = new DVGui::IntLineEdit(this, 0, 0, 16384);
  m_currentWidthFld->setEnabled(false);
  m_currentHeightFld->setEnabled(false);
  grid->addWidget(fieldLabel(tr("Size:")), row, 1);
  grid->addWidget(sizeRowWidget(m_currentWidthFld, m_currentHeightFld), row++,
                  2, Qt::AlignLeft);

  // --- Target Size ---
  sectionTitle(row, tr("Target Size"));

  m_presetCombo = new QComboBox(this);
  loadPresetList();
  m_addPresetBtn    = new QPushButton(tr("Add"), this);
  m_removePresetBtn = new QPushButton(tr("Remove"), this);
  m_addPresetBtn->setObjectName("PushButton_NoPadding");
  m_removePresetBtn->setObjectName("PushButton_NoPadding");
  grid->addWidget(fieldLabel(tr("Preset:")), row, 1);
  grid->addWidget(presetWidget(m_presetCombo, true), row++, 2);

  m_widthFld  = new DVGui::IntLineEdit(this, 1920, 1, 16384);
  m_heightFld = new DVGui::IntLineEdit(this, 1080, 1, 16384);
  m_preserveAspectRatio =
      new DVGui::CheckBox(tr("Preserve aspect ratio"), this);
  m_preserveAspectRatio->setChecked(RescalePopupPreserveAspectRatio != 0);
  grid->addWidget(fieldLabel(tr("Size:")), row, 1);
  grid->addWidget(sizeRowWidget(m_widthFld, m_heightFld, m_preserveAspectRatio),
                  row++, 2, Qt::AlignLeft);

  m_filterCombo = new QComboBox(this);
  m_filterCombo->addItem(tr("Triangle"));
  m_filterCombo->addItem(tr("Mitchell"));
  m_filterCombo->addItem(tr("Lanczos3"));
  m_filterCombo->setFixedHeight(DVGui::WidgetHeight);
  grid->addWidget(fieldLabel(tr("Resample:")), row, 1);
  grid->addWidget(m_filterCombo, row++, 2, Qt::AlignLeft);

  // --- Frame Range ---
  sectionTitle(row, tr("Frame Range"));

  m_fromFld = new DVGui::IntLineEdit(this);
  m_toFld   = new DVGui::IntLineEdit(this);
  m_fromFld->setFixedSize(kFieldWidth, DVGui::WidgetHeight);
  m_toFld->setFixedSize(kFieldWidth, DVGui::WidgetHeight);
  grid->addWidget(fieldLabel(tr("Start:")), row, 1);
  {
    QHBoxLayout *rangeLay = new QHBoxLayout();
    rangeLay->setContentsMargins(0, 0, 0, 0);
    rangeLay->setSpacing(4);
    rangeLay->addWidget(m_fromFld);
    rangeLay->addWidget(new QLabel(tr("End:"), this));
    rangeLay->addWidget(m_toFld);
    rangeLay->addStretch();
    QWidget *box = new QWidget(this);
    box->setLayout(rangeLay);
    grid->addWidget(box, row++, 2, Qt::AlignLeft);
  }

  // --- Output ---
  sectionTitle(row, tr("Output"));

  m_saveInFld = new DVGui::FileField(this, QString());
  grid->addWidget(fieldLabel(tr("Save In:")), row, 1);
  grid->addWidget(m_saveInFld, row++, 2);

  m_fileNameFld = new DVGui::LineEdit(QString(), this);
  m_fileNameFld->setFixedHeight(DVGui::WidgetHeight);
  grid->addWidget(fieldLabel(tr("Level Name:")), row, 1);
  grid->addWidget(m_fileNameFld, row++, 2);

  m_removeDotBeforeFrameNumber =
      new QCheckBox(tr("Remove dot before frame number"), this);
  grid->addWidget(m_removeDotBeforeFrameNumber, row++, 1, 1, 2, Qt::AlignLeft);

  m_skipExisting = new DVGui::CheckBox(tr("Skip Existing Files"), this);
  m_skipExisting->setChecked(RescalePopupSkipExisting != 0);
  grid->addWidget(m_skipExisting, row++, 1, 1, 2, Qt::AlignLeft);

  m_topLayout->addLayout(grid);
  m_topLayout->addSpacing(8);

  m_okBtn     = new QPushButton(tr("Rescale"), this);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  m_okBtn->setDefault(true);

  m_notifier       = new ImageUtils::FrameTaskNotifier();
  m_progressDialog = new DVGui::ProgressDialog("", tr("Cancel"), 0, 0);
  m_progressDialog->setWindowTitle(tr("Rescale"));
  m_progressDialog->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint);
  m_progressDialog->setWindowModality(Qt::WindowModal);

  connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &RescalePopup::onPresetSelected);
  connect(m_widthFld, SIGNAL(editingFinished()), this, SLOT(onWidthEdited()));
  connect(m_heightFld, SIGNAL(editingFinished()), this, SLOT(onHeightEdited()));
  connect(m_preserveAspectRatio, SIGNAL(stateChanged(int)), this,
          SLOT(onPreserveAspectRatioChanged()));
  connect(m_fromFld, SIGNAL(editingFinished()), this, SLOT(onRangeChanged()));
  connect(m_toFld, SIGNAL(editingFinished()), this, SLOT(onRangeChanged()));
  connect(m_addPresetBtn, &QPushButton::clicked, this,
          &RescalePopup::addPreset);
  connect(m_removePresetBtn, &QPushButton::clicked, this,
          &RescalePopup::removePreset);
  connect(m_okBtn, &QPushButton::clicked, this, &RescalePopup::apply);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_progressDialog, SIGNAL(canceled()), m_notifier,
          SLOT(onCancelTask()));
  connect(m_notifier, SIGNAL(frameCompleted(int)), m_progressDialog,
          SLOT(setValue(int)));

  addButtonBarWidget(m_okBtn, m_cancelBtn);

  m_removeDotBeforeFrameNumber->setChecked(RescalePopupRemoveDot != 0);
}

//-----------------------------------------------------------------------------

RescalePopup::~RescalePopup() {
  if (m_worker) {
    m_worker->wait();
    delete m_worker;
  }
  delete m_notifier;
}

//-----------------------------------------------------------------------------

void RescalePopup::loadPresetList() {
  m_presetCombo->clear();
  m_presetCombo->addItem(kCustomPreset);
  QFile file(m_presetListFile);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&file);
    while (!in.atEnd()) {
      QString line = in.readLine().trimmed();
      if (!line.isEmpty()) m_presetCombo->addItem(line);
    }
  }
  m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void RescalePopup::savePresetList() {
  QFile file(m_presetListFile);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream out(&file);
  for (int i = 1; i < m_presetCombo->count(); ++i)
    out << m_presetCombo->itemText(i) << "\n";
}

//-----------------------------------------------------------------------------

void RescalePopup::onPresetSelected(int index) {
  if (m_updatingPreset || index <= 0) return;
  int xres = 0, yres = 0;
  if (!parsePresetResolution(m_presetCombo->itemText(index), xres, yres))
    return;
  m_updatingPreset = true;
  m_widthFld->setValue(xres);
  m_heightFld->setValue(yres);
  m_updatingPreset = false;
}

//-----------------------------------------------------------------------------

void RescalePopup::updatePresetToCustom() {
  if (m_updatingPreset || m_presetCombo->currentIndex() == 0) return;
  m_updatingPreset = true;
  m_presetCombo->setCurrentIndex(0);
  m_updatingPreset = false;
}

//-----------------------------------------------------------------------------

void RescalePopup::syncLinkedSizeFromWidth() {
  if (m_updatingSize || !m_preserveAspectRatio->isChecked() ||
      m_currentSize.lx <= 0 || m_currentSize.ly <= 0)
    return;
  const int w = m_widthFld->getValue();
  if (w <= 0) return;
  m_updatingSize = true;
  const int h    = (int)(w * (double)m_currentSize.ly / m_currentSize.lx + 0.5);
  m_heightFld->setValue(tcrop(h, 1, 16384));
  m_updatingSize = false;
}

//-----------------------------------------------------------------------------

void RescalePopup::syncLinkedSizeFromHeight() {
  if (m_updatingSize || !m_preserveAspectRatio->isChecked() ||
      m_currentSize.lx <= 0 || m_currentSize.ly <= 0)
    return;
  const int h = m_heightFld->getValue();
  if (h <= 0) return;
  m_updatingSize = true;
  const int w    = (int)(h * (double)m_currentSize.lx / m_currentSize.ly + 0.5);
  m_widthFld->setValue(tcrop(w, 1, 16384));
  m_updatingSize = false;
}

//-----------------------------------------------------------------------------

void RescalePopup::onWidthEdited() {
  updatePresetToCustom();
  syncLinkedSizeFromWidth();
}

//-----------------------------------------------------------------------------

void RescalePopup::onHeightEdited() {
  updatePresetToCustom();
  syncLinkedSizeFromHeight();
}

//-----------------------------------------------------------------------------

void RescalePopup::onPreserveAspectRatioChanged() {
  if (m_preserveAspectRatio->isChecked()) syncLinkedSizeFromWidth();
}

//-----------------------------------------------------------------------------

void RescalePopup::addPreset() {
  const int w = m_widthFld->getValue();
  const int h = m_heightFld->getValue();
  if (w <= 0 || h <= 0) {
    DVGui::warning(tr("Target size must be positive."));
    return;
  }

  const QString presetBody = QString::number(w) + "x" + QString::number(h) +
                             ", " + aspectRatioString((double)w / h);

  bool ok      = false;
  QString name = DVGui::getText(
      tr("Preset name"), tr("Enter the name for %1").arg(presetBody), "", &ok);
  if (!ok || name.trimmed().isEmpty()) return;
  if (name.contains(',')) {
    DVGui::warning(tr("The preset name must not contain a comma."));
    return;
  }

  const QString line = name.trimmed() + ", " + presetBody;
  m_presetCombo->addItem(line);
  m_updatingPreset = true;
  m_presetCombo->setCurrentIndex(m_presetCombo->count() - 1);
  m_updatingPreset = false;
  savePresetList();
}

//-----------------------------------------------------------------------------

void RescalePopup::removePreset() {
  const int index = m_presetCombo->currentIndex();
  if (index <= 0) return;
  const int ret = DVGui::MsgBox(
      tr("Deleting \"%1\".\nAre you sure?").arg(m_presetCombo->currentText()),
      tr("Delete"), tr("Cancel"));
  if (ret == 0 || ret == 2) return;
  m_presetCombo->removeItem(index);
  m_presetCombo->setCurrentIndex(0);
  savePresetList();
}

//-----------------------------------------------------------------------------

void RescalePopup::setFiles(const std::vector<TFilePath> &files) {
  m_srcFiles    = files;
  m_currentSize = TDimension();

  if (files.empty()) return;

  m_saveInFld->setPath(
      QString::fromStdWString(files[0].getParentDir().getWideString()));

  ImageUtils::getLevelRasterSize(files[0], m_currentSize);
  updateCurrentSizeDisplay();

  const bool single = files.size() == 1;
  m_fileNameFld->setEnabled(single);

  m_fromFld->setEnabled(false);
  m_toFld->setEnabled(false);
  m_fromFld->setText("");
  m_toFld->setText("");

  if (single) {
    m_fileNameFld->setText(QString::fromStdString(files[0].getName()));
    setWindowTitle(
        tr("Rescale : %1").arg(QString::fromStdString(files[0].getName())));

    TLevelReaderP lr(files[0]);
    if (lr) {
      TLevelP level = lr->loadInfo();
      if (level) {
        TLevel::Table *t = level->getTable();
        if (t && !t->empty()) {
          TFrameId start = t->begin()->first;
          TFrameId end   = t->rbegin()->first;
          if (start.getNumber() >= 0 && end.getNumber() >= 0) {
            m_fromFld->setEnabled(true);
            m_toFld->setEnabled(true);
            m_fromFld->setText(QString::number(start.getNumber()));
            m_toFld->setText(QString::number(end.getNumber()));
          }
        }
      }
    }

    const std::string ext = files[0].getType();
    const bool isSeq      = TFileType::isLevelFilePath(files[0]) &&
                       !TFileType::isLevelExtension(ext);
    m_removeDotBeforeFrameNumber->setEnabled(isSeq);
    if (isSeq && ext == "tga") m_removeDotBeforeFrameNumber->setChecked(true);
  } else {
    setWindowTitle(tr("Rescale %1 Files").arg(files.size()));
    m_fileNameFld->setText("");
    m_removeDotBeforeFrameNumber->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void RescalePopup::updateCurrentSizeDisplay() {
  if (m_currentSize.lx > 0 && m_currentSize.ly > 0) {
    m_currentWidthFld->setValue(m_currentSize.lx);
    m_currentHeightFld->setValue(m_currentSize.ly);
  } else if (m_srcFiles.size() > 1) {
    m_currentWidthFld->setText(tr("—"));
    m_currentHeightFld->setText(tr("—"));
  } else {
    m_currentWidthFld->setText(tr("—"));
    m_currentHeightFld->setText(tr("—"));
  }
}

//-----------------------------------------------------------------------------

void RescalePopup::onRangeChanged() {
  if (m_srcFiles.empty()) return;
  TLevelReaderP lr(m_srcFiles[0]);
  if (!lr) return;
  TLevelP level = lr->loadInfo();
  if (!level) return;
  TLevel::Table *t = level->getTable();
  if (!t || t->empty()) return;
  const TFrameId start = t->begin()->first;
  const TFrameId end   = t->rbegin()->first;
  if (m_toFld->getValue() > end.getNumber())
    m_toFld->setValue(end.getNumber() != -2 ? end.getNumber() : 1);
  if (m_fromFld->getValue() < start.getNumber())
    m_fromFld->setValue(start.getNumber());
}

//-----------------------------------------------------------------------------

TDimension RescalePopup::targetSize() const {
  return TDimension(m_widthFld->getValue(), m_heightFld->getValue());
}

//-----------------------------------------------------------------------------

QString RescalePopup::outputBaseName(const TFilePath &source) const {
  if (m_srcFiles.size() == 1) {
    const QString name = m_fileNameFld->text().trimmed();
    if (!name.isEmpty()) return name;
  }
  return QString::fromStdString(source.getName());
}

//-----------------------------------------------------------------------------

TFilePath RescalePopup::destinationPath(const TFilePath &source) const {
  TFilePath destFolder = source.getParentDir();
  ToonzScene *scene    = TApp::instance()->getCurrentScene()->getScene();
  if (!m_saveInFld->getPath().isEmpty()) {
    destFolder =
        scene->decodeFilePath(TFilePath(m_saveInFld->getPath().toStdWString()));
  }

  const std::wstring name = outputBaseName(source).toStdWString();
  TFilePath destName      = TFilePath(name).withType(source.getType());

  if (TFileType::isLevelFilePath(source) &&
      !TFileType::isLevelExtension(source.getType())) {
    TOutputProperties *prop = scene->getProperties()->getOutputProperties();
    destName                = destName.withFrame(prop->formatTemplateFId());
  }

  return destFolder + destName;
}

//-----------------------------------------------------------------------------

void RescalePopup::getFrameRange(const TFilePath &source, TFrameId &from,
                                 TFrameId &to) const {
  from = to = TFrameId();

  if (!TFileType::isLevelFilePath(source)) return;

  TLevelReaderP lr(source);
  if (!lr) return;
  TLevelP level = lr->loadInfo();
  if (!level) return;
  TLevel::Table *t = level->getTable();
  if (!t || t->empty()) return;

  TFrameId firstFrame = from = t->begin()->first;
  TFrameId lastFrame = to = t->rbegin()->first;

  if (m_srcFiles.size() == 1 && m_fromFld->isEnabled() &&
      m_toFld->isEnabled()) {
    bool ok      = false;
    TFrameId fid = TFrameId(m_fromFld->text().toInt(&ok));
    if (ok && fid > from) from = tcrop(fid, firstFrame, lastFrame);
    fid = TFrameId(m_toFld->text().toInt(&ok));
    if (ok && fid < to) to = tcrop(fid, firstFrame, lastFrame);
  }
}

//-----------------------------------------------------------------------------

bool RescalePopup::checkParameters() const {
  if (m_srcFiles.empty()) return false;
  if (targetSize().lx <= 0 || targetSize().ly <= 0) {
    DVGui::warning(tr("Target size must be positive."));
    return false;
  }
  if (m_saveInFld->getPath().isEmpty()) {
    DVGui::warning(tr("Please select an output folder."));
    return false;
  }
  if (m_srcFiles.size() == 1 && m_fileNameFld->text().trimmed().isEmpty()) {
    DVGui::warning(
        tr("No output level name specified: please choose a valid name."));
    return false;
  }
  for (const TFilePath &fp : m_srcFiles) {
    if (!ImageUtils::isRescalable(fp)) {
      DVGui::warning(tr("Unsupported file type: %1")
                         .arg(QString::fromStdWString(fp.getWideString())));
      return false;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

void RescalePopup::apply() {
  if (!checkParameters() || m_isRunning) return;

  RescalePopupSkipExisting = m_skipExisting->isChecked() ? 1 : 0;
  RescalePopupRemoveDot    = m_removeDotBeforeFrameNumber->isChecked() ? 1 : 0;
  RescalePopupPreserveAspectRatio = m_preserveAspectRatio->isChecked() ? 1 : 0;

  m_isRunning = true;
  m_okBtn->setEnabled(false);
  m_notifier->reset();
  m_progressDialog->setValue(0);
  m_progressDialog->show();

  if (m_worker) {
    m_worker->wait();
    delete m_worker;
  }
  m_worker = new Worker(this);
  connect(m_worker, &QThread::finished, this, &RescalePopup::onFinished);
  m_worker->start();
}

//-----------------------------------------------------------------------------

void RescalePopup::onFinished() {
  m_progressDialog->hide();
  m_okBtn->setEnabled(true);
  m_isRunning = false;
  if (m_notifier->getErrorCount() == 0)
    DVGui::info(tr("Rescale completed."));
  else
    DVGui::warning(tr("Rescale finished with errors."));
  if (!m_srcFiles.empty()) {
    const TFilePath outFolder(m_saveInFld->getPath().toStdWString());
    FileBrowser::refreshFolder(outFolder);
    const TFilePath srcFolder = m_srcFiles[0].getParentDir();
    if (outFolder != srcFolder) FileBrowser::refreshFolder(srcFolder);
  }
  accept();
}

//=============================================================================

OpenPopupCommandHandler<RescalePopup> openRescalePopup(MI_RescaleFiles);
