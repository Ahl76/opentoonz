

#include "filebrowser.h"

// Tnz6 includes
#include "dvdirtreeview.h"
#include "filebrowsermodel.h"
#include "fileselection.h"
#include "filmstripselection.h"
#include "castselection.h"
#include "menubarcommandids.h"
#include "floatingpanelcommand.h"
#include "iocommand.h"
#include "history.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/infoviewer.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/trepetitionguard.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/namebuilder.h"
#include "toonz/toonzimageutils.h"
#include "toonzqt/imageutils.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"

// TnzBase includes
#include "tfiletype.h"
#include "tenv.h"

#include <functional>

// TnzCore includes
#include "tsystem.h"
#include "tconvert.h"
#include "tfiletype.h"
#include "tlevel_io.h"

// Qt includes
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QResizeEvent>
#include <QBoxLayout>
#include <QLabel>
#include <QByteArray>
#include <QMenu>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QDesktopServices>
#include <QDirModel>
#include <QDir>
#include <QPixmap>
#include <QUrl>
#include <QScrollBar>
#include <QScrollArea>
#include <QMap>
#include <QPushButton>
#include <QToolButton>
#include <QPalette>
#include <QCheckBox>
#include <QMutex>
#include <QMutexLocker>
#include <QMessageBox>
#include <QApplication>
#include <QFormLayout>
#include <QMainWindow>
#include <QLineEdit>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QFileSystemWatcher>
#include <QHash>
#include <QTimer>
#include <QSettings>

// tcg includes
#include "tcg/boost/range_utility.h"
#include "tcg/boost/permuted_range.h"

// boost includes
#include <boost/iterator/counting_iterator.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>

// C++ includes
#include <algorithm>
#include <memory>  // std::unique_ptr, std::make_unique

namespace ba = boost::adaptors;

namespace {

//! Theme SVG at \p size (cached).
QPixmap browserThemeSvgIcon(const QString &iconName, const QSize &size) {
  static QHash<QString, QPixmap> cache;
  const QString key = iconName + QLatin1Char('@') + QString::number(size.width()) +
                      QLatin1Char('x') + QString::number(size.height());
  const auto it = cache.constFind(key);
  if (it != cache.cend()) return it.value();
  QPixmap pm =
      svgToPixmap(getIconPath(iconName), size, Qt::KeepAspectRatio, Qt::transparent);
  cache.insert(key, pm);
  return pm;
}

}  // namespace

//=============================================================================
//    BrowserFileSettings
//-----------------------------------------------------------------------------

BrowserFileSettings *BrowserFileSettings::instance() {
  static BrowserFileSettings _instance;
  return &_instance;
}

BrowserFileSettings::BrowserFileSettings() {}

void BrowserFileSettings::ensureLoaded() {
  static bool loaded = false;
  if (loaded) return;
  loaded = true;
  load();
}

QString BrowserFileSettings::pathKey(const TFilePath &path) {
  return path.getQString();
}

void BrowserFileSettings::load() {
  m_bgOverrides.clear();
  m_favorites.clear();
  const TFilePath fp =
      ToonzFolder::getMyModuleDir() + TFilePath("BrowserFileSettings.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("ThumbnailBg"));
  const QStringList paths = settings.childKeys();
  for (const QString &key : paths) {
    bool ok    = false;
    const int v = settings.value(key).toInt(&ok);
    if (ok) m_bgOverrides.insert(key, v);
  }
  settings.endGroup();
  for (const QVariant &v :
       settings.value(QStringLiteral("Favorites")).toList())
    m_favorites.insert(v.toString());
}

void BrowserFileSettings::save() const {
  const TFilePath fp =
      ToonzFolder::getMyModuleDir() + TFilePath("BrowserFileSettings.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.remove(QString());
  settings.beginGroup(QStringLiteral("ThumbnailBg"));
  for (auto it = m_bgOverrides.constBegin(); it != m_bgOverrides.constEnd();
       ++it)
    settings.setValue(it.key(), it.value());
  settings.endGroup();
  QStringList favList = m_favorites.values();
  favList.sort();
  QList<QVariant> favVar;
  for (const QString &s : favList) favVar.append(s);
  settings.setValue(QStringLiteral("Favorites"), favVar);
}

int BrowserFileSettings::thumbnailBgOverride(const TFilePath &path) const {
  BrowserFileSettings::instance()->ensureLoaded();
  const auto it = m_bgOverrides.constFind(pathKey(path));
  return it == m_bgOverrides.constEnd() ? -1 : it.value();
}

void BrowserFileSettings::setThumbnailBgOverride(const TFilePath &path,
                                                 int mode) {
  ensureLoaded();
  const QString key = pathKey(path);
  if (mode < 0)
    m_bgOverrides.remove(key);
  else
    m_bgOverrides.insert(key, mode);
  save();
}

void BrowserFileSettings::clearThumbnailBgOverride(const TFilePath &path) {
  setThumbnailBgOverride(path, -1);
}

bool BrowserFileSettings::isFavorite(const TFilePath &path) const {
  BrowserFileSettings::instance()->ensureLoaded();
  return m_favorites.contains(pathKey(path));
}

void BrowserFileSettings::setFavorite(const TFilePath &path, bool on) {
  ensureLoaded();
  const QString key = pathKey(path);
  if (on)
    m_favorites.insert(key);
  else
    m_favorites.remove(key);
  save();
}

void BrowserFileSettings::toggleFavorite(const TFilePath &path) {
  setFavorite(path, !isFavorite(path));
}

//-----------------------------------------------------------------------------

bool supportsBrowserThumbnailCustomization(const TFilePath &path) {
  if (TFileStatus(path).isDirectory()) return false;
  const TFileType::Type type = TFileType::getInfo(path);
  return TFileType::isViewable(type) || TFileType::isScene(type);
}

//-----------------------------------------------------------------------------

bool supportsBrowserFavorites(const TFilePath &path) {
  if (TFileStatus(path).isDirectory()) return false;
  const TFileType::Type type = TFileType::getInfo(path);
  return TFileType::isViewable(type) || TFileType::isScene(type) ||
         TFileType::isLevel(type);
}

//-----------------------------------------------------------------------------

void appendThumbnailBackgroundMenu(
    QMenu *parentMenu, const std::function<void(int)> &onModeSelected) {
  QMenu *bgMenu = parentMenu->addMenu(QObject::tr("Thumbnail Background"));
  auto addBgAct = [&](const char *iconName, const QString &label, int mode) {
    QAction *a = iconName ? bgMenu->addAction(createQIcon(iconName), label)
                          : bgMenu->addAction(label);
    QObject::connect(a, &QAction::triggered, parentMenu,
                     [onModeSelected, mode]() { onModeSelected(mode); });
  };
  addBgAct(nullptr, QObject::tr("Use Default"), -1);
  bgMenu->addSeparator();
  addBgAct("browser_preview_white", QObject::tr("White Background"), 1);
  addBgAct("browser_preview_black", QObject::tr("Black Background"), 2);
  addBgAct("browser_preview_transparency",
           QObject::tr("Transparent Background"), 0);
  addBgAct("browser_preview_checkboard", QObject::tr("Checkered Background"),
           3);
}

using namespace DVGui;

//=============================================================================
//      Local declarations
//=============================================================================

//=============================================================================
//    FrameCountTask class
//-----------------------------------------------------------------------------

class FrameCountTask final : public TThread::Runnable {
  bool m_started;

  TFilePath m_path;
  QDateTime m_modifiedDate;

public:
  FrameCountTask(const TFilePath &path, const QDateTime &modifiedDate);
  ~FrameCountTask();

  void run() override;
  QThread::Priority runningPriority() override;

public slots:
  void onStarted(TThread::RunnableP thisTask) override;
  void onCanceled(TThread::RunnableP thisTask) override;
};

//=============================================================================
//    FCData struct
//-----------------------------------------------------------------------------

struct FCData {
  QDateTime m_date;
  int m_frameCount;
  bool m_underProgress;
  int m_retryCount;

  FCData() = default;
  explicit FCData(const QDateTime &date);
};

//=============================================================================
//      Local namespace
//=============================================================================

namespace {
std::set<FileBrowser *> activeBrowsers;
std::map<TFilePath, FCData> frameCountMap;
QMutex frameCountMapMutex;
QMutex levelFileMutex;
TEnv::IntVar BrowserInfoPanelVisible("BrowserInfoPanelVisible", 0);
TEnv::IntVar BrowserInfoPanelWidth("BrowserInfoPanelWidth", 220);

QPixmap peekAnyBgIcon(const TFilePath &fp, const TDimension &dim,
                      const TFrameId &fid, int preferBg) {
  QPixmap px =
      IconGenerator::instance()->peekSizedIcon(fp, dim, fid, preferBg);
  if (!px.isNull()) return px;
  for (int m = 0; m <= (int)DvItemViewerPanel::BgAuto; ++m) {
    if (m == preferBg) continue;
    px = IconGenerator::instance()->peekSizedIcon(fp, dim, fid, m);
    if (!px.isNull()) return px;
  }
  return QPixmap();
}
}  // namespace

//=============================================================================
// FileBrowser
//-----------------------------------------------------------------------------

FileBrowser::FileBrowser(QWidget *parent, Qt::WindowFlags flags,
                         bool noContextMenu, bool multiSelectionEnabled)
    : QFrame(parent), m_folderName(nullptr), m_itemViewer(nullptr) {
  // style sheet
  setObjectName("FileBrowser");
  setFrameStyle(QFrame::StyledPanel);

  m_mainSplitter      = new QSplitter(this);
  m_folderTreeView    = new DvDirTreeView(this);
  QFrame *box         = new QFrame(this);
  QLabel *folderLabel = new QLabel(tr("Folder: "), this);
  m_folderName        = new QLineEdit(this);
  m_itemViewer = new DvItemViewer(box, noContextMenu, multiSelectionEnabled,
                                  DvItemViewer::Browser);
  DvItemViewerTitleBar *titleBar = new DvItemViewerTitleBar(m_itemViewer, box);
  DvItemViewerButtonBar *buttonBar =
      new DvItemViewerButtonBar(m_itemViewer, box);
  m_buttonBar = buttonBar;
  DvItemViewerPanel *viewerPanel = m_itemViewer->getPanel();

  viewerPanel->addColumn(DvItemListModel::FileType, 50);
  viewerPanel->addColumn(DvItemListModel::FrameCount, 50);
  viewerPanel->addColumn(DvItemListModel::FileSize, 50);
  viewerPanel->addColumn(DvItemListModel::CreationDate, 130);
  viewerPanel->addColumn(DvItemListModel::ModifiedDate, 130);
  if (Preferences::instance()->isSVNEnabled())
    viewerPanel->addColumn(DvItemListModel::VersionControlStatus, 120);

  viewerPanel->setSelection(new FileSelection());
  DVItemViewPlayDelegate *itemViewPlayDelegate =
      new DVItemViewPlayDelegate(viewerPanel);
  viewerPanel->setItemViewPlayDelegate(itemViewPlayDelegate);

  connect(viewerPanel, &DvItemViewerPanel::thumbnailBgModeChanged, this,
          [this](int) {
            if (m_infoCurrentPath != TFilePath())
              updateInfoThumbnail(m_infoCurrentPath);
          });

  m_itemsSplitter = new QSplitter(Qt::Horizontal, box);
  m_itemsSplitter->setObjectName("FileBrowserItemsSplitter");
  m_itemsSplitter->setChildrenCollapsible(false);

  m_infoScrollArea = new QScrollArea(m_itemsSplitter);
  m_infoScrollArea->setObjectName("FileBrowserInfoScroll");
  m_infoScrollArea->setWidgetResizable(true);
  m_infoScrollArea->setFrameShape(QFrame::NoFrame);
  m_infoScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_infoScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_infoScrollArea->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_infoScrollArea->setMinimumWidth(140);
  m_infoScrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  m_infoPanelHost = new QWidget();
  m_infoPanelHost->setMinimumWidth(0);
  m_infoPanelHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  auto *infoPanelLayout = new QVBoxLayout(m_infoPanelHost);
  infoPanelLayout->setContentsMargins(0, 0, 0, 0);
  infoPanelLayout->setSpacing(0);
  infoPanelLayout->setAlignment(Qt::AlignTop);

  auto *thumbHeader = new QHBoxLayout();
  thumbHeader->setContentsMargins(4, 4, 4, 0);
  thumbHeader->setSpacing(2);
  m_thumbCollapseBtn = new QToolButton();
  m_thumbCollapseBtn->setArrowType(Qt::DownArrow);
  m_thumbCollapseBtn->setFixedSize(16, 16);
  m_thumbCollapseBtn->setAutoRaise(true);
  m_thumbCollapseBtn->setToolTip(tr("Show/Hide Thumbnail"));
  thumbHeader->addWidget(m_thumbCollapseBtn);
  thumbHeader->addStretch();
  infoPanelLayout->addLayout(thumbHeader);

  m_infoThumbnail = new QLabel();
  m_infoThumbnail->setAlignment(Qt::AlignCenter);
  m_infoThumbnail->setMinimumHeight(20);
  m_infoThumbnail->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  infoPanelLayout->addWidget(m_infoThumbnail, 0, Qt::AlignTop);

  connect(m_thumbCollapseBtn, &QToolButton::clicked, this, [this]() {
    m_infoThumbVisible = !m_infoThumbVisible;
    m_infoThumbnail->setVisible(m_infoThumbVisible);
    m_thumbCollapseBtn->setArrowType(m_infoThumbVisible ? Qt::DownArrow
                                                        : Qt::RightArrow);
  });

  m_infoViewer = new InfoViewer();
  infoPanelLayout->addWidget(m_infoViewer, 0, Qt::AlignTop);
  m_infoViewer->setEmbedded(true);
  connect(m_infoViewer, &InfoViewer::currentFrameChanged, this, [this]() {
    if (m_infoCurrentPath != TFilePath()) updateInfoThumbnail(m_infoCurrentPath);
  });

  connect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
          &FileBrowser::onIconGenerated);

  m_infoScrollArea->setWidget(m_infoPanelHost);

  // Right-click → Hide (when the Advanced Info button is unavailable).
  auto wireInfoHideMenu = [this](QWidget *w) {
    if (!w) return;
    w->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(w, &QWidget::customContextMenuRequested, this,
            &FileBrowser::onInfoPanelContextMenu, Qt::UniqueConnection);
  };
  wireInfoHideMenu(m_infoScrollArea);
  wireInfoHideMenu(m_infoScrollArea->viewport());
  for (QWidget *w : m_infoScrollArea->findChildren<QWidget *>())
    wireInfoHideMenu(w);

  connect(m_itemsSplitter, &QSplitter::splitterMoved, this,
          &FileBrowser::onItemsSplitterMoved);

  m_mainSplitter->setObjectName("FileBrowserSplitter");
  m_folderTreeView->setObjectName("DirTreeView");
  box->setObjectName("castFrame");
  box->setFrameStyle(QFrame::StyledPanel);

  m_itemViewer->setModel(this);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setContentsMargins(3, 3, 3, 3);
  mainLayout->setSpacing(2);
  {
    mainLayout->addWidget(buttonBar);

    QHBoxLayout *folderLay = new QHBoxLayout();
    folderLay->setContentsMargins(0, 0, 0, 0);
    folderLay->setSpacing(0);
    {
      folderLay->addWidget(folderLabel, 0);
      folderLay->addWidget(m_folderName, 1);
    }
    mainLayout->addLayout(folderLay, 0);

    m_mainSplitter->addWidget(m_folderTreeView);
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(0, 0, 0, 0);
    boxLayout->setSpacing(0);
    {
      boxLayout->addWidget(titleBar, 0);
      m_itemsSplitter->addWidget(m_itemViewer);
      m_itemsSplitter->addWidget(m_infoScrollArea);
      m_infoScrollArea->setVisible(false);
      m_itemsSplitter->setStretchFactor(0, 1);
      m_itemsSplitter->setStretchFactor(1, 0);
      boxLayout->addWidget(m_itemsSplitter, 1);
    }
    m_mainSplitter->addWidget(box);
    mainLayout->addWidget(m_mainSplitter, 1);
  }
  setLayout(mainLayout);

  m_mainSplitter->setSizes(QList<int>() << 270 << 500);

  connect(m_folderTreeView, &DvDirTreeView::currentNodeChanged,
          itemViewPlayDelegate, &DVItemViewPlayDelegate::resetPlayWidget);
  connect(m_folderTreeView, &DvDirTreeView::currentNodeChanged, this,
          &FileBrowser::onTreeFolderChanged);

  connect(m_itemViewer, &DvItemViewer::clickedItem, this,
          &FileBrowser::onClickedItem);
  connect(m_itemViewer, &DvItemViewer::doubleClickedItem, this,
          &FileBrowser::onDoubleClickedItem);
  connect(m_itemViewer, &DvItemViewer::selectedItems, this,
          &FileBrowser::onSelectedItems);
  connect(buttonBar, &DvItemViewerButtonBar::folderUp, this,
          &FileBrowser::folderUp);
  connect(buttonBar, &DvItemViewerButtonBar::newFolder, this,
          &FileBrowser::newFolder);
  connect(buttonBar, &DvItemViewerButtonBar::searchFilterChanged, this,
          &FileBrowser::onSearchFilterChanged);
  connect(buttonBar, &DvItemViewerButtonBar::typeFilterChanged, this,
          &FileBrowser::onTypeFilterChanged);
  connect(buttonBar, &DvItemViewerButtonBar::favoritesFilterChanged, this,
          &FileBrowser::onFavoritesFilterChanged);
  connect(buttonBar, &DvItemViewerButtonBar::projectFolderTriggered, this,
          [this](const TFilePath &fp) { setFolder(fp, true); });
  if (QAction *infoAct = buttonBar->infoPanelAction()) {
    connect(infoAct, SIGNAL(triggered(bool)), this,
            SLOT(onInfoPanelActionTriggered(bool)));
  }

  connect(&m_frameCountReader, &FrameCountReader::calculatedFrameCount,
          m_itemViewer->getPanel(), qOverload<>(&DvItemViewerPanel::update));

  QAction *refresh = CommandManager::instance()->getAction(MI_RefreshTree);
  connect(refresh, SIGNAL(triggered()), this, SLOT(refresh()));
  addAction(refresh);

  // Version Control instance connection
  if (Preferences::instance()->isSVNEnabled()) {
    connect(VersionControl::instance(), &VersionControl::commandDone, this,
            &FileBrowser::onVersionControlCommandDone);
  }

  // if the folderName is edited, move the current folder accordingly
  connect(m_folderName, &QLineEdit::editingFinished, this,
          &FileBrowser::onFolderEdited);

  // folder history
  connect(m_folderTreeView, &DvDirTreeView::currentNodeChanged, this,
          &FileBrowser::storeFolderHistory);
  connect(buttonBar, &DvItemViewerButtonBar::folderBack, this,
          &FileBrowser::onBackButtonPushed);
  connect(buttonBar, &DvItemViewerButtonBar::folderFwd, this,
          &FileBrowser::onFwdButtonPushed);
  // when the history changes, enable/disable the history buttons accordingly
  connect(this, &FileBrowser::historyChanged, buttonBar,
          &DvItemViewerButtonBar::onHistoryChanged);

  // check out the update of the current folder.
  // Use MyFileSystemWatcher which is shared by all browsers.
  // Adding and removing paths to the watcher is done in DvDirTreeView.
  connect(MyFileSystemWatcher::instance(),
          &MyFileSystemWatcher::directoryChanged, this,
          &FileBrowser::onFileSystemChanged);

  // store the first item("Root") in the history
  m_indexHistoryList.append(m_folderTreeView->currentIndex());
  m_currentPosition = 0;

  refreshHistoryButtons();

  if (BrowserInfoPanelVisible) {
    buttonBar->setInfoPanelChecked(true);
    setInfoPanelVisible(true);
  }

  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          buttonBar, &DvItemViewerButtonBar::refreshProjectFolderShortcuts);
  buttonBar->refreshProjectFolderShortcuts();
}

//-----------------------------------------------------------------------------

FileBrowser::~FileBrowser() = default;  // all child widgets are auto-deleted

//-----------------------------------------------------------------------------

void FileBrowser::save(QSettings &settings) const {
  if (m_mainSplitter)
    settings.setValue("treeSplitterState", m_mainSplitter->saveState());
  settings.setValue("infoPanelVisible", m_infoPanelVisible ? 1 : 0);
  int infoW = (int)BrowserInfoPanelWidth;
  if (m_infoPanelVisible && m_itemsSplitter) {
    const QList<int> sizes = m_itemsSplitter->sizes();
    if (sizes.size() == 2 && sizes[1] > 0) infoW = sizes[1];
  }
  if (infoW > 0) settings.setValue("infoPanelWidth", infoW);
}

//-----------------------------------------------------------------------------

void FileBrowser::load(QSettings &settings) {
  if (m_mainSplitter) {
    const QByteArray state = settings.value("treeSplitterState").toByteArray();
    if (!state.isEmpty()) m_mainSplitter->restoreState(state);
  }
  if (settings.contains("infoPanelWidth")) {
    const int w = settings.value("infoPanelWidth").toInt();
    if (w >= 140) BrowserInfoPanelWidth = w;
  }
  if (settings.contains("infoPanelVisible")) {
    const bool vis = settings.value("infoPanelVisible").toInt() != 0;
    if (m_buttonBar) m_buttonBar->setInfoPanelChecked(vis);
    if (m_infoPanelVisible != vis)
      setInfoPanelVisible(vis);
    else if (vis)
      applyInfoPanelSize();
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::onFolderEdited() {
  TFilePath inputPath(m_folderName->text().toStdWString());
  QModelIndex index = DvDirModel::instance()->getIndexByPath(inputPath);

  // If there is no node matched
  if (!index.isValid()) {
    QMessageBox::warning(this, tr("Open folder failed"),
                         tr("The input folder path was invalid."));
    return;
  }
  m_folderTreeView->collapseAll();

  m_folderTreeView->setCurrentIndex(index);

  // expand the folder tree
  QModelIndex tmpIndex = index;
  while (tmpIndex.isValid()) {
    m_folderTreeView->expand(tmpIndex);
    tmpIndex = tmpIndex.parent();
  }

  m_folderTreeView->scrollTo(index);
  m_folderTreeView->update();
}

//-----------------------------------------------------------------------------

void FileBrowser::storeFolderHistory() {
  QModelIndex currentModelIndex = m_folderTreeView->currentIndex();

  if (!currentModelIndex.isValid()) return;
  if (m_indexHistoryList[m_currentPosition] == currentModelIndex) return;

  // If there is no next history item, then create it
  if (m_currentPosition == m_indexHistoryList.size() - 1) {
    m_indexHistoryList << currentModelIndex;
    ++m_currentPosition;
  }
  // If the next history item is the same as the current one, just move to it
  else if (m_indexHistoryList[m_currentPosition + 1] == currentModelIndex) {
    ++m_currentPosition;
  }
  // If the next history item is different from the current one, then replace
  // with the new one
  else {
    int size = m_indexHistoryList.size();
    // remove the old history items
    for (int i = m_currentPosition + 1; i < size; ++i)
      m_indexHistoryList.removeLast();
    m_indexHistoryList << currentModelIndex;
    ++m_currentPosition;
  }
  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshHistoryButtons() {
  emit historyChanged(m_currentPosition != 0,
                      m_currentPosition != m_indexHistoryList.size() - 1);
}

//-----------------------------------------------------------------------------

void FileBrowser::onBackButtonPushed() {
  if (m_currentPosition == 0) return;
  --m_currentPosition;
  QModelIndex currentIndex = m_indexHistoryList[m_currentPosition];
  m_folderTreeView->setCurrentIndex(currentIndex);
  m_folderTreeView->collapseAll();
  while (currentIndex.isValid()) {
    currentIndex = currentIndex.parent();
    m_folderTreeView->expand(currentIndex);
  }
  m_folderTreeView->update();
  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------

void FileBrowser::onFwdButtonPushed() {
  if (m_currentPosition >= m_indexHistoryList.size() - 1) return;
  ++m_currentPosition;
  QModelIndex currentIndex = m_indexHistoryList[m_currentPosition];
  m_folderTreeView->setCurrentIndex(currentIndex);
  m_folderTreeView->collapseAll();
  while (currentIndex.isValid()) {
    currentIndex = currentIndex.parent();
    m_folderTreeView->expand(currentIndex);
  }
  m_folderTreeView->update();
  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------
/*! clear the history when the tree date is replaced
 */
void FileBrowser::clearHistory() {
  int size = m_indexHistoryList.size();
  // leave the last item
  for (int i = 1; i < size; ++i) m_indexHistoryList.removeLast();
  m_currentPosition = 0;
  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------
/*! update the current folder when changes detected from QFileSystemWatcher
 */
void FileBrowser::onFileSystemChanged(const QString &folderPath) {
  if (folderPath != m_folder.getQString()) return;
  // changes may create/delete of folder, so update the DvDirModel
  QModelIndex parentFolderIndex = m_folderTreeView->currentIndex();
  DvDirModel::instance()->refresh(parentFolderIndex);

  refreshCurrentFolderItems();
}

//-----------------------------------------------------------------------------

void FileBrowser::sortByDataModel(DataType dataType, bool isDiscendent) {
  struct locals {
    static inline bool itemLess(int aIdx, int bIdx, FileBrowser &fb,
                                DataType dataType) {
      return fb.compareData(dataType, aIdx, bIdx) > 0;
    }

    static inline bool indexLess(int aIdx, int bIdx,
                                 const std::vector<int> &vec) {
      return vec[aIdx] < vec[bIdx];
    }

    static inline int complement(int val, int max) {
      return (assert(0 <= val && val <= max), max - val);
    }
  };

  if (dataType != getCurrentOrderType()) {
    // Build the permutation table
    std::vector<int> new2OldIdx(
        boost::make_counting_iterator(0),
        boost::make_counting_iterator(int(m_items.size())));

    std::stable_sort(new2OldIdx.begin(), new2OldIdx.end(),
                     [this, dataType](int x, int y) {
                       return locals::itemLess(x, y, *this, dataType);
                     });

    // Use the renumbering table to permutate elements
    std::vector<Item>(
        boost::make_permutation_iterator(m_items.begin(), new2OldIdx.begin()),
        boost::make_permutation_iterator(m_items.begin(), new2OldIdx.end()))
        .swap(m_items);

    // Use the permutation table to update current file selection, if any
    FileSelection *fs =
        static_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());

    if (!fs->isEmpty()) {
      std::vector<int> old2NewIdx(
          boost::make_counting_iterator(0),
          boost::make_counting_iterator(int(m_items.size())));

      std::sort(old2NewIdx.begin(), old2NewIdx.end(),
                [&new2OldIdx](int aIdx, int bIdx) {
                  return locals::indexLess(aIdx, bIdx, new2OldIdx);
                });

      std::vector<int> newSelectedIndices;
      tcg::substitute(newSelectedIndices,
                      tcg::permuted_range(
                          old2NewIdx, fs->getSelectedIndices() |
                                          ba::filtered([&old2NewIdx](int x) {
                                            return x < old2NewIdx.size();
                                          })));

      fs->select(!newSelectedIndices.empty() ? &newSelectedIndices.front() : 0,
                 int(newSelectedIndices.size()));
    }

    setIsDiscendentOrder(true);
    setOrderType(dataType);
  }

  // Reverse lists if necessary
  if (isDiscendentOrder() != isDiscendent) {
    std::reverse(m_items.begin(), m_items.end());

    // Reverse file selection, if any
    FileSelection *fs =
        dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());

    if (!fs || fs->isEmpty()) {
      // nothing to do
    } else {
      int iCount = int(m_items.size()), lastIdx = iCount - 1;

      std::vector<int> newSelectedIndices;
      tcg::substitute(newSelectedIndices,
                      fs->getSelectedIndices() | ba::filtered([iCount](int x) {
                        return x < iCount;
                      }) | ba::transformed([lastIdx](int x) {
                        return locals::complement(x, lastIdx);
                      }));

      fs->select(!newSelectedIndices.empty() ? &newSelectedIndices.front() : 0,
                 int(newSelectedIndices.size()));
    }

    setIsDiscendentOrder(isDiscendent);
  }

  // Folders stay above files after sorting.
  storePersistedSelection();
  pinFoldersFirst();
  restorePersistedSelection();

  m_itemViewer->getPanel()->update();
}

//-----------------------------------------------------------------------------

void FileBrowser::setFilterTypes(const QStringList &types) { m_filter = types; }

//-----------------------------------------------------------------------------

void FileBrowser::addFilterType(const QString &type) {
  if (!m_filter.contains(type)) m_filter.push_back(type);
}

//-----------------------------------------------------------------------------

void FileBrowser::removeFilterType(const QString &type) {
  m_filter.removeAll(type);
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshCurrentFolderItems() {
  m_items.clear();

  // put the parent directory item
  TFilePath parentFp = m_folder.getParentDir();
  if (parentFp != TFilePath("") && parentFp != m_folder)
    m_items.emplace_back(parentFp, true, false);

  // register all folder items by using the folder tree model
  DvDirModel *model        = DvDirModel::instance();
  QModelIndex currentIndex = model->getIndexByPath(m_folder);
  if (currentIndex.isValid()) {
    for (int i = 0; i < model->rowCount(currentIndex); ++i) {
      QModelIndex tmpIndex = model->index(i, 0, currentIndex);
      if (tmpIndex.isValid()) {
        DvDirModelFileFolderNode *node =
            dynamic_cast<DvDirModelFileFolderNode *>(model->getNode(tmpIndex));
        if (node) {
          TFilePath childFolderPath = node->getPath();
          if (TFileStatus(childFolderPath).isLink())
            m_items.emplace_back(childFolderPath, true, true,
                                 QString::fromStdWString(node->getName()));
          else
            m_items.emplace_back(childFolderPath, true, false,
                                 QString::fromStdWString(node->getName()));
        }
      }
    }
  } else
    setUnregisteredFolder(m_folder);

  // register the file items
  if (m_folder != TFilePath()) {
    TFilePathSet files;
    TFilePathSet all_files;  // for updating m_multiFileItemMap

    TFileStatus fpStatus(m_folder);
    // if the item is link, then set the link target of it
    if (fpStatus.isLink()) {
      QFileInfo info(toQString(m_folder));
      setFolder(TFilePath(info.symLinkTarget().toStdWString()));
      return;
    }
    if (fpStatus.doesExist() && fpStatus.isDirectory() &&
        fpStatus.isReadable()) {
      try {
        TSystem::readDirectory(files, all_files, m_folder);
      } catch (...) {
      }
    }

    for (const TFilePath &it : files) {
#ifdef _WIN32
      // include folder shortcut items
      if (it.getType() == "lnk") {
        TFileStatus info(it);
        if (info.isLink() && info.isDirectory()) {
          m_items.emplace_back(it, true, true,
                               QString::fromStdString(it.getName()));
        }
        continue;
      }
#endif
      // skip the plt file (Palette file for TOONZ 4.6 and earlier)
      if (it.getType() == "plt") continue;

      // filter the file
      else if (m_filter.isEmpty()) {
        if (it.getType() != "tnz" && it.getType() != "scr" &&
            it.getType() != "tnzbat" && it.getType() != "mpath" &&
            it.getType() != "curve" && it.getType() != "tpl" &&
            TFileType::getInfo(it) == TFileType::UNKNOW_FILE)
          continue;
      } else if (!m_filter.contains(QString::fromStdString(it.getType())))
        continue;
      // store the filtered file paths
      m_items.emplace_back(it);
    }

    // update the m_multiFileItemMap
    m_multiFileItemMap.clear();

    for (const TFilePath &it : all_files) {
      TFrameId tFrameId = it.getFrame();
      TFilePath levelName(it.getLevelName());

      if (levelName.isLevelName()) {
        Item &levelItem = m_multiFileItemMap[levelName];

        // TODO:
        // For now, leave it as is, but if obtaining FileInfo takes too much
        // time, consider making it optional 2015/12/28 shun_iwasawa
        QFileInfo fileInfo(QString::fromStdWString(it.getWideString()));
        // Get creation time safely across platforms
        QDateTime creationTime = fileInfo.birthTime();
        if (!creationTime.isValid())
          creationTime = fileInfo.metadataChangeTime();

        // Update level infos
        if (levelItem.m_creationDate.isNull() ||
            creationTime < levelItem.m_creationDate)
          levelItem.m_creationDate = creationTime;

        if (levelItem.m_modifiedDate.isNull() ||
            fileInfo.lastModified() > levelItem.m_modifiedDate)
          levelItem.m_modifiedDate = fileInfo.lastModified();

        levelItem.m_fileSize += fileInfo.size();

        // Store frame ID
        levelItem.m_frameIds.push_back(tFrameId);
        levelItem.m_frameCount++;
      }
    }
  }

  // Set Missing Version Control Items
  int missingItemCount          = 0;
  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (node) {
    QList<TFilePath> list = node->getMissingFiles();
    missingItemCount      = list.size();
    for (int i = 0; i < missingItemCount; ++i) m_items.emplace_back(list.at(i));
  }

  // Refresh Data (fill Item field)
  refreshData();

  // If I added some missing items I need to sort items.
  if (missingItemCount > 0) {
    DataType currentDataType = getCurrentOrderType();
    for (int i = 1; i < (int)m_items.size(); ++i) {
      int index = i;
      while (index > 0 && compareData(currentDataType, index - 1, index) > 0) {
        std::swap(m_items[index - 1], m_items[index]);
        --index;
      }
    }
  }
  // Keep the full listing, then apply name/type/favorites filters.
  m_folderItems = m_items;
  applyNameFilter();
}

//-----------------------------------------------------------------------------

void FileBrowser::setFolder(const TFilePath &fp, bool expandNode,
                            bool forceUpdate) {
  if (fp == m_folder && !forceUpdate) return;

  // set the current folder path
  m_folder        = fp;
  m_dayDateString = "";
  // set the folder name
  if (fp == TFilePath())
    m_folderName->setText("");
  else
    m_folderName->setText(toQString(fp));

  refreshCurrentFolderItems();

  if (!TFileStatus(fp).isLink())
    m_folderTreeView->setCurrentNode(fp, expandNode);
}

//-----------------------------------------------------------------------------
/*! process when inputting the folder which is not registered in the folder tree
   (e.g. UNC path in Windows)
 */
void FileBrowser::setUnregisteredFolder(const TFilePath &fp) {
  if (fp != TFilePath()) {
    TFileStatus fpStatus(fp);
    // if the item is link, then set the link target of it
    if (fpStatus.isLink()) {
      QFileInfo info(toQString(fp));
      setFolder(TFilePath(info.symLinkTarget().toStdWString()));
      return;
    }

    // get both the folder & file list by readDirectory and
    // readDirectory_Dir_ReadExe
    TFilePathSet folders;
    TFilePathSet files;
    // for updating m_multiFileItemMap
    TFilePathSet all_files;

    if (fpStatus.doesExist() && fpStatus.isDirectory() &&
        fpStatus.isReadable()) {
      try {
        TSystem::readDirectory(files, all_files, fp);
        TSystem::readDirectory_Dir_ReadExe(folders, fp);
      } catch (...) {
      }
    }

    // register all folder items
    for (const TFilePath &it : folders) {
      if (TFileStatus(it).isLink())
        m_items.emplace_back(it, true, true);
      else
        m_items.emplace_back(it, true, false);
    }

    for (const TFilePath &it : files) {
#ifdef _WIN32
      // include folder shortcut items
      if (it.getType() == "lnk") {
        TFileStatus info(it);
        if (info.isLink() && info.isDirectory()) {
          m_items.emplace_back(it, true, true,
                               QString::fromStdString(it.getName()));
        }
        continue;
      }
#endif
      // skip the plt file (Palette file for TOONZ 4.6 and earlier)
      if (it.getType() == "plt") continue;

      // filtering
      else if (m_filter.isEmpty()) {
        if (it.getType() != "tnz" && it.getType() != "scr" &&
            it.getType() != "tnzbat" && it.getType() != "mpath" &&
            it.getType() != "curve" && it.getType() != "tpl" &&
            TFileType::getInfo(it) == TFileType::UNKNOW_FILE)
          continue;
      } else if (!m_filter.contains(QString::fromStdString(it.getType())))
        continue;

      m_items.emplace_back(it);
    }

    // update the m_multiFileItemMap
    m_multiFileItemMap.clear();
    for (const TFilePath &it : all_files) {
      TFilePath levelName(it.getLevelName());
      if (levelName.isLevelName()) {
        Item &levelItem = m_multiFileItemMap[levelName];
        levelItem.m_frameIds.push_back(it.getFrame());
        levelItem.m_frameCount++;
      }
    }
  }
  // for all items in the folder, retrieve the file names(m_name) from the
  // paths(m_path)
  refreshData();

  m_folderItems = m_items;
  applyNameFilter();
  m_itemViewer->repaint();
}

//-----------------------------------------------------------------------------

void FileBrowser::setHistoryDay(std::string dayDateString) {
  m_folder                = TFilePath();
  m_dayDateString         = dayDateString;
  const History::Day *day = History::instance()->getDay(dayDateString);
  m_items.clear();
  if (day == nullptr) {
    m_folderName->setText("");
  } else {
    m_folderName->setText(QString::fromStdString(dayDateString));
    std::vector<TFilePath> files;
    day->getFiles(files);
    for (const TFilePath &it : files) m_items.emplace_back(it);
  }
  refreshData();
  m_folderItems = m_items;
  applyNameFilter();
}

//-----------------------------------------------------------------------------
/*! for all items in the folder, retrieve the file names(m_name) from the
 * paths(m_path)
 */
void FileBrowser::refreshData() {
  for (Item &it : m_items) {
    if (it.m_name.isEmpty())
      it.m_name = toQString(it.m_path.withoutParentDir());
  }
}

//-----------------------------------------------------------------------------

int FileBrowser::getItemCount() const { return int(m_items.size()); }

//-----------------------------------------------------------------------------

void FileBrowser::readInfo(Item &item) {
  TFilePath fp = item.m_path;
  QFileInfo info(toQString(fp));
  if (info.exists()) {
    // Use birthTime(), fall back to metadataChangeTime() if unavailable
    item.m_creationDate = info.birthTime();
    if (!item.m_creationDate.isValid())
      item.m_creationDate = info.metadataChangeTime();
    item.m_modifiedDate = info.lastModified();
    item.m_fileType     = info.suffix();
    item.m_fileSize     = info.size();
    if (fp.getType() == "tnz") {
      ToonzScene scene;
      try {
        item.m_frameCount = scene.loadFrameCount(fp);
      } catch (...) {
      }
    } else
      readFrameCount(item);

    item.m_validInfo = true;
  } else if (fp.isLevelName()) {
    try {
      // Find this level's item
      auto it = m_multiFileItemMap.find(TFilePath(item.m_path.getLevelName()));
      if (it == m_multiFileItemMap.end()) throw "";

      item.m_creationDate = it->second.m_creationDate;
      item.m_modifiedDate = it->second.m_modifiedDate;
      item.m_fileType     = it->second.m_fileType;
      item.m_fileSize     = it->second.m_fileSize;
      item.m_frameCount   = it->second.m_frameCount;
      item.m_validInfo    = true;

      // keep the list of frameIds at the first time and try to reuse it.
      item.m_frameIds = it->second.m_frameIds;
    } catch (...) {
      item.m_frameCount = 0;
    }
  }

  item.m_validInfo = true;
}

//-----------------------------------------------------------------------------

//! Frame count needs a special access function for viewable types - for they
//! are calculated by using a dedicated thread and therefore cannot be simply
//! classified as *valid* or *invalid* infos...
void FileBrowser::readFrameCount(Item &item) {
  if (!item.m_isFolder &&
      TFileType::isViewable(TFileType::getInfo(item.m_path))) {
    if (isMultipleFrameType(item.m_path.getType()))
      item.m_frameCount = m_frameCountReader.getFrameCount(item.m_path);
    else
      item.m_frameCount = 1;
  } else
    item.m_frameCount = 0;
}

//-----------------------------------------------------------------------------

QVariant FileBrowser::getItemData(int index, DataType dataType,
                                  bool isSelected) {
  if (index < 0 || index >= getItemCount()) return QVariant();
  Item &item = m_items[index];
  if (dataType == Name) {
    // show two dots( ".." ) for the parent directory item
    if (item.m_path == m_folder.getParentDir())
      return QString("..");
    else
      return item.m_name;
  } else if (dataType == Thumbnail) {
    DvItemViewerPanel *panel = m_itemViewer->getPanel();
    QSize iconSize           = panel->getIconSize();
    QSize renderSize         = panel->getRenderIconSize();
    // Folder icons: render SVG at the live cell size.
    if (item.m_path == m_folder.getParentDir()) {
      return browserThemeSvgIcon(QStringLiteral("folder_browser_up"), iconSize);
    } else if (item.m_isFolder) {
      if (item.m_isLink)
        return browserThemeSvgIcon(QStringLiteral("folder_browser_link"),
                                   iconSize);
      return browserThemeSvgIcon(QStringLiteral("folder_browser"), iconSize);
    }

    // Request committed renderSize; peek previous size while it generates.
    QPixmap pixmap;
    const int bgMode = [&]() {
      int mode = panel->isAdvancedDisplay()
                     ? (int)panel->getThumbnailBgMode()
                     : 0;
      if (panel->isAdvancedDisplay() &&
          supportsBrowserThumbnailCustomization(item.m_path)) {
        const int ov =
            BrowserFileSettings::instance()->thumbnailBgOverride(item.m_path);
        if (ov >= 0) mode = ov;
      }
      return mode;
    }();
    if (panel->isAdvancedDisplay() && renderSize.width() > 0 &&
        renderSize.height() > 0) {
      const qreal dpr = qMax(1.0, panel->devicePixelRatioF());
      const TDimension phys = TDimension(
          qMax(1, qRound(renderSize.width() * dpr)),
          qMax(1, qRound(renderSize.height() * dpr)));
      pixmap = IconGenerator::instance()->getSizedIcon(
          item.m_path, phys, TFrameId::NO_FRAME, bgMode);
      if (pixmap.isNull()) {
        const QSize prev = panel->getPrevRenderIconSize();
        if (prev.width() > 0 && prev.height() > 0 && prev != renderSize) {
          pixmap = peekAnyBgIcon(
              item.m_path,
              TDimension(qMax(1, qRound(prev.width() * dpr)),
                         qMax(1, qRound(prev.height() * dpr))),
              TFrameId::NO_FRAME, bgMode);
        }
      }
      if (pixmap.isNull()) {
        pixmap = peekAnyBgIcon(item.m_path, phys, TFrameId::NO_FRAME, bgMode);
      }
      if (!pixmap.isNull() && dpr > 1.0) pixmap.setDevicePixelRatio(dpr);
    }
    if (pixmap.isNull())
      pixmap = IconGenerator::instance()->getIcon(item.m_path);
    if (pixmap.isNull()) {
      pixmap = QPixmap(iconSize);
      pixmap.fill(Qt::transparent);
    }
    if (panel->isAdvancedDisplay()) return pixmap;
    return scalePixmapKeepingAspectRatio(pixmap, iconSize, Qt::transparent);
  } else if (dataType == Icon)
    return QVariant();
  else if (dataType == ToolTip || dataType == FullPath)
    return QString::fromStdWString(item.m_path.getWideString());

  else if (dataType == IsFolder)
    return item.m_isFolder;
  else if (dataType == IsFavorite)
    return supportsBrowserFavorites(item.m_path) &&
           BrowserFileSettings::instance()->isFavorite(item.m_path);
  else if (dataType == ThumbnailBg) {
    if (!supportsBrowserThumbnailCustomization(item.m_path)) return QVariant();
    const int ov =
        BrowserFileSettings::instance()->thumbnailBgOverride(item.m_path);
    if (ov >= 0) return ov;
    return QVariant();
  }

  if (!item.m_validInfo) {
    readInfo(item);
    if (!item.m_validInfo) return QVariant();
  }

  if (dataType == CreationDate) return item.m_creationDate;
  if (dataType == ModifiedDate) return item.m_modifiedDate;
  if (dataType == FileType) {
    if (item.m_isLink)
      return QString("<LINK>");
    else if (item.m_isFolder)
      return QString("<DIR>");
    else
      return QString::fromStdString(item.m_path.getType()).toUpper();
  } else if (dataType == FileSize)
    return (item.m_fileSize == -1) ? QVariant() : item.m_fileSize;
  else if (dataType == FrameCount) {
    if (item.m_frameCount == -1) readFrameCount(item);
    return item.m_frameCount;
  } else if (dataType == PlayAvailable) {
    std::string type = item.m_path.getType();
    if (item.m_frameCount > 1 && type != "tzp" && type != "tzu") return true;
    return false;
  } else if (dataType == VersionControlStatus) {
    return getItemVersionControlStatus(item);
  } else
    return QVariant();
}

//-----------------------------------------------------------------------------

bool FileBrowser::isSceneItem(int index) const {
  return 0 <= index && index < getItemCount() &&
         m_items[index].m_path.getType() == "tnz";
}

//-----------------------------------------------------------------------------

bool FileBrowser::canRenameItem(int index) const {
  // if viewing history, cannot rename anything
  if (getFolder() == TFilePath()) return false;
  if (index < 0 || index >= getItemCount()) return false;
  // for now, disable rename for folders
  if (m_items[index].m_isFolder) return false;
  TFilePath fp = m_items[index].m_path;
  return TFileStatus(fp).doesExist();
}

//-----------------------------------------------------------------------------

int FileBrowser::findIndexWithPath(TFilePath path) {
  for (int i = 0; i < (int)m_items.size(); ++i)
    if (m_items[i].m_path == path) return i;
#ifdef _WIN32
  // Windows paths are case-insensitive; FullPath casing may differ after refresh.
  const QString target =
      QString::fromStdWString(path.getWideString()).toLower();
  for (int i = 0; i < (int)m_items.size(); ++i) {
    if (QString::fromStdWString(m_items[i].m_path.getWideString()).toLower() ==
        target)
      return i;
  }
#endif
  return -1;
}

//-----------------------------------------------------------------------------

void FileBrowser::storePersistedSelection() {
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  // Ignore empty clears so a later restore still has paths.
  if (!fs || fs->isEmpty()) return;
  const std::set<int> &indices = fs->getSelectedIndices();
  std::vector<TFilePath> paths;
  paths.reserve(indices.size());
  for (int idx : indices) {
    if (idx < 0 || idx >= (int)m_items.size()) continue;
    paths.push_back(m_items[idx].m_path);
  }
  if (!paths.empty()) m_persistedSelection.swap(paths);
}

//-----------------------------------------------------------------------------

void FileBrowser::restorePersistedSelection() {
  if (m_persistedSelection.empty()) return;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;

  std::vector<int> indices;
  indices.reserve(m_persistedSelection.size());
  for (const TFilePath &fp : m_persistedSelection) {
    const int idx = findIndexWithPath(fp);
    if (idx >= 0) indices.push_back(idx);
  }
  if (indices.empty()) return;
  fs->select(&indices.front(), (int)indices.size());
  fs->makeCurrent();
  m_itemViewer->getPanel()->update();
}

//-----------------------------------------------------------------------------

void FileBrowser::pinFoldersFirst() {
  if (m_items.empty()) return;

  Item parent;
  bool hasParent = false;
  std::vector<Item> folders, files;
  folders.reserve(m_items.size());
  files.reserve(m_items.size());

  for (const Item &item : m_items) {
    if (!m_folder.isEmpty() && item.m_path == m_folder.getParentDir()) {
      parent    = item;
      hasParent = true;
    } else if (item.m_isFolder) {
      folders.push_back(item);
    } else {
      files.push_back(item);
    }
  }

  std::stable_sort(folders.begin(), folders.end(),
                   [](const Item &a, const Item &b) {
                     return QString::localeAwareCompare(a.m_name, b.m_name) < 0;
                   });

  m_items.clear();
  if (hasParent) m_items.push_back(parent);
  m_items.insert(m_items.end(), folders.begin(), folders.end());
  m_items.insert(m_items.end(), files.begin(), files.end());
}

//-----------------------------------------------------------------------------

void FileBrowser::applyNameFilter() {
  storePersistedSelection();
  if (FileSelection *fs = dynamic_cast<FileSelection *>(
          m_itemViewer->getPanel()->getSelection()))
    fs->selectNone();

  m_items.clear();
  m_items.reserve(m_folderItems.size());

  for (const Item &item : m_folderItems) {
    const bool isParent =
        !m_folder.isEmpty() && item.m_path == m_folder.getParentDir();
    const bool isFolder = isParent || item.m_isFolder;

    const QString name =
        item.m_name.isEmpty()
            ? QString::fromStdWString(item.m_path.getLevelNameW())
            : item.m_name;

    // ".." entry is always shown; other folders follow the name filter.
    if (isFolder) {
      if (!isParent && !m_nameFilter.isEmpty() &&
          !name.contains(m_nameFilter, Qt::CaseInsensitive))
        continue;
      m_items.push_back(item);
      continue;
    }

    if (!m_typeFilter.isEmpty()) {
      const QString ext =
          QString::fromStdString(item.m_path.getType()).toUpper();
      if (!m_typeFilter.contains(ext)) continue;
    }

    if (!m_nameFilter.isEmpty() &&
        !name.contains(m_nameFilter, Qt::CaseInsensitive))
      continue;

    if (m_favoritesOnly &&
        !BrowserFileSettings::instance()->isFavorite(item.m_path))
      continue;

    m_items.push_back(item);
  }

  bool discendentOrder     = isDiscendentOrder();
  DataType currentDataType = getCurrentOrderType();
  setOrderType(Name);
  setIsDiscendentOrder(true);
  sortByDataModel(currentDataType, discendentOrder);

  restorePersistedSelection();
  if (m_itemViewer) {
    m_itemViewer->updateContentSize();
    m_itemViewer->refresh();
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::onSearchFilterChanged(const QString &text) {
  m_nameFilter = text.trimmed();
  if (m_folderItems.empty() && !m_items.empty()) m_folderItems = m_items;
  applyNameFilter();
}

//-----------------------------------------------------------------------------

void FileBrowser::onFavoritesFilterChanged(bool on) {
  m_favoritesOnly = on;
  if (m_folderItems.empty() && !m_items.empty()) m_folderItems = m_items;
  applyNameFilter();
}

//-----------------------------------------------------------------------------

void FileBrowser::setSelectedThumbnailBg(int mode) {
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  std::vector<TFilePath> files;
  fs->getSelectedFiles(files);
  for (const TFilePath &fp : files) {
    if (!supportsBrowserThumbnailCustomization(fp)) continue;
    if (mode < 0)
      BrowserFileSettings::instance()->clearThumbnailBgOverride(fp);
    else
      BrowserFileSettings::instance()->setThumbnailBgOverride(fp, mode);
  }
  updateItemViewerPanel();
  if (m_infoCurrentPath != TFilePath()) updateInfoThumbnail(m_infoCurrentPath);
}

//-----------------------------------------------------------------------------

void FileBrowser::toggleSelectedFavorite() {
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  std::vector<TFilePath> files;
  fs->getSelectedFiles(files);
  for (const TFilePath &fp : files) {
    if (!supportsBrowserFavorites(fp)) continue;
    BrowserFileSettings::instance()->toggleFavorite(fp);
  }
  if (m_favoritesOnly) applyNameFilter();
  else
    updateItemViewerPanel();
}

//-----------------------------------------------------------------------------

void FileBrowser::onTypeFilterChanged(const QStringList &extensions) {
  m_typeFilter.clear();
  for (const QString &ext : extensions) {
    const QString u = ext.trimmed().toUpper();
    if (!u.isEmpty()) m_typeFilter.insert(u);
  }
  if (m_folderItems.empty() && !m_items.empty()) m_folderItems = m_items;
  applyNameFilter();
}

//-----------------------------------------------------------------------------

void FileBrowser::renameItem(int index, const QString &newName) {
  if (getFolder() == TFilePath()) return;
  if (index < 0 || index >= (int)m_items.size()) return;

  TFilePath fp    = m_items[index].m_path;
  TFilePath newFp = fp;
  if (renameFile(newFp, newName)) {
    m_items[index].m_name = QString::fromStdWString(newFp.getLevelNameW());
    m_items[index].m_path = newFp;

    // I have also renamed the palette, I must update it.
    if (newFp.getType() == "tlv" || newFp.getType() == "tzp" ||
        newFp.getType() == "tzu") {
      const char *type = (newFp.getType() == "tlv") ? "tpl" : "plt";
      int paletteIndex = findIndexWithPath(fp.withNoFrame().withType(type));
      if (paletteIndex >= 0) {
        TFilePath palettePath = newFp.withNoFrame().withType(type);
        m_items[paletteIndex].m_name =
            QString::fromStdWString(palettePath.getLevelNameW());
        m_items[paletteIndex].m_path = palettePath;
      }
    }
    m_itemViewer->update();

    if (fp.getType() == "tnz") {
      // I changed the _files folder. I must update the folder that contains it
      // in the tree view
      QModelIndex index = m_folderTreeView->currentIndex();
      if (index.isValid()) {
        DvDirModel::instance()->refresh(index);
        m_folderTreeView->update();
      }
    }
  }
}

//-----------------------------------------------------------------------------

bool FileBrowser::renameFile(TFilePath &fp, QString newName) {
  if (isSpaceString(newName)) return true;

  TFilePath newFp(newName.toStdWString());
  if (!newFp.getType().empty() && newFp.getType() != fp.getType()) {
    DVGui::error(tr("Can't change file extension"));
    return false;
  }
  if (newFp.getType().empty()) newFp = newFp.withType(fp.getType());
  if (newFp.getFrame() != TFrameId::EMPTY_FRAME &&
      newFp.getFrame() != TFrameId::NO_FRAME) {
    DVGui::error(tr("Can't set a drawing number"));
    return false;
  }
  if (newFp.getDots() != fp.getDots()) {
    if (fp.getDots() == ".")
      newFp = newFp.withNoFrame();
    else if (fp.getDots() == "..")
      newFp = newFp.withFrame(TFrameId::EMPTY_FRAME);
  }
  newFp = newFp.withParentDir(fp.getParentDir());

  // if they are the same, I don't need to rename anything
  if (newFp == fp) return false;

  if (TSystem::doesExistFileOrLevel(newFp)) {
    DVGui::error(tr("Can't rename. File already exists: ") + toQString(newFp));
    return false;
  }

  try {
    TSystem::renameFileOrLevel_throw(newFp, fp, true);
    IconGenerator::instance()->remove(fp);
    if (fp.getType() == "tnz") {
      TFilePath sceneIconFp    = ToonzScene::getIconPath(fp);
      TFilePath sceneIconNewFp = ToonzScene::getIconPath(newFp);
      if (TFileStatus(sceneIconFp).doesExist()) {
        if (TFileStatus(sceneIconNewFp).doesExist())
          TSystem::deleteFile(sceneIconNewFp);
        TSystem::renameFile(sceneIconNewFp, sceneIconFp);
      }
    }

  } catch (...) {
    DVGui::error(tr("Couldn't rename ") + toQString(fp) + " to " +
                 toQString(newFp));
    return false;
  }

  fp = newFp;
  return true;
}

//-----------------------------------------------------------------------------

QMenu *FileBrowser::getContextMenu(QWidget *parent, int index) {
  auto isOldLevelType = [](const TFilePath &path) -> bool {
    return path.getType() == "tzp" || path.getType() == "tzu";
  };

  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return nullptr;
  std::vector<TFilePath> files;
  fs->getSelectedFiles(files);

  // Use unique_ptr for automatic memory management
  auto menu          = std::make_unique<QMenu>(parent);
  CommandManager *cm = CommandManager::instance();

  // when folder item is right-clicked
  if (0 <= index && index < (int)m_items.size() && m_items[index].m_isFolder) {
    DvDirModel *model    = DvDirModel::instance();
    DvDirModelNode *node = model->getNode(model->getIndexByPath(files[0]));

    if (!node || !node->isRenameEnabled()) return nullptr;

    DvDirModelFileFolderNode *fnode =
        dynamic_cast<DvDirModelFileFolderNode *>(node);
    if (!fnode || fnode->isProjectFolder()) return nullptr;

    menu->addAction(cm->getAction(MI_Clear));
    QAction *action =
        menu->addAction(QIcon(createQIcon("rename")), tr("Rename"));
    connect(action, &QAction::triggered, this, &FileBrowser::renameFolder);
    return menu.release();
  }

  if (files.empty()) {
    menu->addAction(cm->getAction(MI_ShowFolderContents));
    menu->addAction(cm->getAction(MI_SelectAll));
    if (!Preferences::instance()->isWatchFileSystemEnabled()) {
      menu->addAction(cm->getAction(MI_RefreshTree));
    }
    return menu.release();
  }

  if (files.size() == 1 && files[0].getType() == "tnz") {
    menu->addAction(cm->getAction(MI_LoadScene));
  }

  bool areResources = true;
  bool areScenes    = false;
  for (const TFilePath &f : files) {
    TFileType::Type type = TFileType::getInfo(f);
    if (areResources && !TFileType::isResource(type)) areResources = false;
    if (!areScenes && TFileType::isScene(type)) areScenes = true;
  }

  bool areFullcolor = true;
  for (const TFilePath &f : files) {
    TFileType::Type type = TFileType::getInfo(f);
    if (!TFileType::isFullColor(type)) {
      areFullcolor = false;
      break;
    }
  }

  TFilePath clickedFile;
  if (0 <= index && index < (int)m_items.size())
    clickedFile = m_items[index].m_path;

  if (areResources) {
    QString title;
    if (clickedFile != TFilePath() && clickedFile.getType() == "tnz")
      title = tr("Load As Sub-xsheet");
    else
      title = tr("Load");
    QAction *action = menu->addAction(QIcon(createQIcon("import")), title);
    connect(action, &QAction::triggered, this, &FileBrowser::loadResources);
    menu->addSeparator();
  }

  menu->addAction(cm->getAction(MI_DuplicateFile));
  if (!areScenes) {
    menu->addAction(cm->getAction(MI_Copy));
    menu->addAction(cm->getAction(MI_Paste));
  }
  menu->addAction(cm->getAction(MI_Clear));
  menu->addAction(cm->getAction(MI_ShowFolderContents));
  menu->addAction(cm->getAction(MI_SelectAll));
  menu->addAction(cm->getAction(MI_FileInfo));
  if (!clickedFile.isEmpty() &&
      (clickedFile.getType() == "tnz" || clickedFile.getType() == "tab")) {
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_AddToBatchRenderList));
    menu->addAction(cm->getAction(MI_AddToBatchCleanupList));
  }

  int i;
  for (i = 0; i < (int)files.size(); ++i)
    if (!TFileType::isViewable(TFileType::getInfo(files[i])) &&
        files[i].getType() != "tpl")
      break;
  if (i == (int)files.size()) {
    std::string type = files[0].getType();
    int j;
    for (j = 0; j < (int)files.size(); ++j)
      if (isOldLevelType(files[j])) break;
    if (j == (int)files.size()) menu->addAction(cm->getAction(MI_ViewFile));

    for (j = 0; j < (int)files.size(); ++j) {
      if ((files[0].getType() == "pli" && files[j].getType() != "pli") ||
          (files[0].getType() != "pli" && files[j].getType() == "pli"))
        break;
      else if ((isOldLevelType(files[0]) && !isOldLevelType(files[j])) ||
               (!isOldLevelType(files[0]) && isOldLevelType(files[j])))
        break;
    }
    if (j == (int)files.size()) {
      bool allRescalable = true;
      for (const TFilePath &f : files) {
        if (!ImageUtils::isRescalable(f)) {
          allRescalable = false;
          break;
        }
      }
      if (allRescalable && files[0].getType() != "pli")
        menu->addAction(cm->getAction(MI_RescaleFiles));
      menu->addAction(cm->getAction(MI_ConvertFiles));
    }
    if (areFullcolor) menu->addAction(cm->getAction(MI_SeparateColors));

    if (files.size() == 1 && files[0].getType() != "tnz") {
      QAction *action =
          menu->addAction(QIcon(createQIcon("rename")), tr("Rename"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::renameAsToonzLevel);
    }

    if (!areFullcolor) menu->addSeparator();
  }

#ifdef LEVO

  if (files.size() == 2 &&
      (files[0].getType() == "tif" || files[0].getType() == "tiff" ||
       files[0].getType() == "png" || files[0].getType() == "TIF" ||
       files[0].getType() == "TIFF" || files[0].getType() == "PNG") &&
      (files[1].getType() == "tif" || files[1].getType() == "tiff" ||
       files[1].getType() == "png" || files[1].getType() == "TIF" ||
       files[1].getType() == "TIFF" || files[1].getType() == "PNG")) {
    QAction *action = new QAction(tr("Convert to Painted TLV"), menu);
    connect(action, &QAction::triggered, this,
            &FileBrowser::convertToPaintedTlv);
    menu->addAction(action);
  }
  if (areFullcolor) {
    QAction *action = new QAction(tr("Convert to Unpainted TLV"), menu);
    connect(action, &QAction::triggered, this,
            &FileBrowser::convertToUnpaintedTlv);
    menu->addAction(action);
    menu->addSeparator();
  }
#endif

  if (!clickedFile.isEmpty() && (clickedFile.getType() == "tnz")) {
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_CollectAssets));
    menu->addAction(cm->getAction(MI_ImportScenes));
    menu->addAction(cm->getAction(MI_ExportScenes));
  }

  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (node) {
    // Check Version Control Status
    DvItemListModel::Status status = DvItemListModel::Status(
        m_itemViewer->getModel()
            ->getItemData(index, DvItemListModel::VersionControlStatus)
            .toInt());

    // Remove the added actions
    if (status == DvItemListModel::VC_Missing) menu->clear();

    auto vcMenu     = std::make_unique<QMenu>(tr("Version Control"), parent);
    QAction *action = nullptr;

    if (status == DvItemListModel::VC_ReadOnly ||
        (status == DvItemListModel::VC_ToUpdate && files.size() == 1)) {
      if (status == DvItemListModel::VC_ReadOnly) {
        action = vcMenu->addAction(tr("Edit"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::editVersionControl);

        TFilePath path       = files.at(0);
        std::string fileType = path.getType();
        if (fileType == "tlv" || fileType == "pli" || path.getDots() == "..") {
          action = vcMenu->addAction(tr("Edit Frame Range..."));
          connect(action, &QAction::triggered, this,
                  &FileBrowser::editFrameRangeVersionControl);
        }
      } else {
        action = vcMenu->addAction(tr("Edit"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::updateAndEditVersionControl);
      }
    }

    if (status == DvItemListModel::VC_Modified) {
      action = vcMenu->addAction(tr("Put..."));
      connect(action, &QAction::triggered, this,
              &FileBrowser::putVersionControl);

      action = vcMenu->addAction(tr("Revert"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::revertVersionControl);
    }

    if (status == DvItemListModel::VC_ReadOnly ||
        status == DvItemListModel::VC_ToUpdate) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (status == DvItemListModel::VC_ReadOnly) {
        action = vcMenu->addAction(tr("Delete"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::deleteVersionControl);
      }

      vcMenu->addSeparator();

      if (files.size() == 1) {
        action         = vcMenu->addAction(tr("Get Revision..."));
        TFilePath path = files.at(0);
        if (path.getDots() == "..")
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionVersionControl);
        else
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionHistory);
      } else if (files.size() > 1) {
        action = vcMenu->addAction("Get Revision...");
        connect(action, &QAction::triggered, this,
                &FileBrowser::getRevisionVersionControl);
      }
    }

    if (status == DvItemListModel::VC_Edited) {
      action = vcMenu->addAction(tr("Unlock"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::unlockVersionControl);
    }

    if (status == DvItemListModel::VC_Unversioned) {
      action = vcMenu->addAction(tr("Put..."));
      connect(action, &QAction::triggered, this,
              &FileBrowser::putVersionControl);
    }

    if (status == DvItemListModel::VC_Locked && files.size() == 1) {
      action = vcMenu->addAction(tr("Unlock"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::unlockVersionControl);

      action = vcMenu->addAction(tr("Edit Info"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::showLockInformation);
    }

    if (status == DvItemListModel::VC_Missing) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (files.size() == 1) {
        vcMenu->addSeparator();
        action         = vcMenu->addAction(tr("Revision History..."));
        TFilePath path = files.at(0);
        if (path.getDots() == "..")
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionVersionControl);
        else
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionHistory);
      }
    }

    if (status == DvItemListModel::VC_PartialLocked) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);
      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Edit Frame Range..."));
        connect(action, &QAction::triggered, this,
                &FileBrowser::editFrameRangeVersionControl);

        action = vcMenu->addAction(tr("Edit Info"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::showFrameRangeLockInfo);
      }
    } else if (status == DvItemListModel::VC_PartialEdited) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Unlock Frame Range"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::unlockFrameRangeVersionControl);

        action = vcMenu->addAction(tr("Edit Info"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::showFrameRangeLockInfo);
      }
    } else if (status == DvItemListModel::VC_PartialModified) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Put..."));
        connect(action, &QAction::triggered, this,
                &FileBrowser::putFrameRangeVersionControl);

        action = vcMenu->addAction(tr("Revert"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::revertFrameRangeVersionControl);
      }
    }

    if (!vcMenu->isEmpty()) {
      menu->addSeparator();
      menu->addMenu(vcMenu.release());  // transfer ownership to menu
    }
  }

  if (!Preferences::instance()->isWatchFileSystemEnabled()) {
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_RefreshTree));
  }

  {
    bool hasThumbItem = false;
    bool hasFavItem   = false;
    for (const TFilePath &f : files) {
      if (supportsBrowserThumbnailCustomization(f)) hasThumbItem = true;
      if (supportsBrowserFavorites(f)) hasFavItem = true;
    }
    DvItemViewerPanel *panel = m_itemViewer->getPanel();
    if (panel && panel->isAdvancedDisplay() && (hasThumbItem || hasFavItem)) {
      menu->addSeparator();
      if (hasThumbItem) {
        appendThumbnailBackgroundMenu(menu.get(), [this](int mode) {
          setSelectedThumbnailBg(mode);
        });
      }

      if (hasFavItem) {
        bool allFav = !files.empty();
        for (const TFilePath &f : files) {
          if (!supportsBrowserFavorites(f)) {
            allFav = false;
            break;
          }
          if (!BrowserFileSettings::instance()->isFavorite(f)) {
            allFav = false;
            break;
          }
        }
        QAction *favAct = menu->addAction(
            createQIcon("star"),
            allFav ? tr("Remove from Favorites") : tr("Add to Favorites"));
        connect(favAct, &QAction::triggered, this,
                &FileBrowser::toggleSelectedFavorite);
      }
    }
  }

  return menu.release();
}

//-----------------------------------------------------------------------------

void FileBrowser::startDragDrop() {
  TRepetitionGuard guard;
  if (!guard.hasLock()) return;

  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  std::vector<TFilePath> files;
  fs->getSelectedFiles(files);
  if (files.empty()) return;

  QList<QUrl> urls;
  for (const TFilePath &f : files) {
    if (TSystem::doesExistFileOrLevel(f))
      urls.append(
          QUrl::fromLocalFile(QString::fromStdWString(f.getWideString())));
  }
  if (urls.isEmpty()) return;

  QMimeData *mimeData = new QMimeData;
  mimeData->setUrls(urls);
  QDrag *drag    = new QDrag(this);
  QSize iconSize = m_itemViewer->getPanel()->getIconSize();
  QPixmap icon   = IconGenerator::instance()->getIcon(files[0]);
  QPixmap dropThumbnail =
      scalePixmapKeepingAspectRatio(icon, iconSize, Qt::transparent);
  if (!dropThumbnail.isNull()) drag->setPixmap(dropThumbnail);
  drag->setMimeData(mimeData);

  drag->exec(Qt::CopyAction);
}

//-----------------------------------------------------------------------------

bool FileBrowser::dropMimeData(QTreeWidgetItem *, int, const QMimeData *,
                               Qt::DropAction) {
  return false;
}

//-----------------------------------------------------------------------------

void FileBrowser::onTreeFolderChanged() {
  DvDirModelNode *node = m_folderTreeView->getCurrentNode();
  if (node)
    node->visualizeContent(this);
  else
    setFolder(TFilePath());
  m_itemViewer->resetVerticalScrollBar();
  m_itemViewer->updateContentSize();
  m_itemViewer->getPanel()->update();
  m_frameCountReader.stopReading();
  IconGenerator::instance()->clearRequests();

  DvDirModelFileFolderNode *fileFolderNode =
      dynamic_cast<DvDirModelFileFolderNode *>(node);
  if (fileFolderNode) emit treeFolderChanged(fileFolderNode->getPath());
}

//-----------------------------------------------------------------------------

void FileBrowser::changeFolder(const QModelIndex &) {}

//-----------------------------------------------------------------------------

void FileBrowser::onDataChanged(const QModelIndex &, const QModelIndex &) {
  onTreeFolderChanged();
}

//-----------------------------------------------------------------------------

bool FileBrowser::acceptDrop(const QMimeData *data) const {
  // if the browser is not displaying a standard folder, cannot accept any drop
  if (getFolder() == TFilePath()) return false;

  if (data->hasFormat("application/vnd.toonz.levels") ||
      data->hasFormat("application/vnd.toonz.currentscene") ||
      data->hasFormat("application/vnd.toonz.drawings") ||
      acceptResourceDrop(data->urls()))
    return true;

  return false;
}

//-----------------------------------------------------------------------------

bool FileBrowser::drop(const QMimeData *mimeData) {
  // if the browser is not displaying a standard folder, cannot accept any drop
  TFilePath folderPath = getFolder();
  if (folderPath == TFilePath()) return false;

  if (mimeData->hasFormat(CastItems::getMimeFormat())) {
    const CastItems *items = dynamic_cast<const CastItems *>(mimeData);
    if (!items) return false;

    for (int i = 0; i < items->getItemCount(); ++i) {
      CastItem *item = items->getItem(i);
      if (TXshSimpleLevel *sl = item->getSimpleLevel()) {
        TFilePath levelPath = sl->getPath().withParentDir(getFolder());
        IoCmd::saveLevel(levelPath, sl, false);
      } else if (TXshSoundLevel *level = item->getSoundLevel()) {
        TFilePath soundPath = level->getPath().withParentDir(getFolder());
        IoCmd::saveSound(soundPath, level, false);
      }
    }
    refreshFolder(getFolder());
    return true;
  } else if (mimeData->hasFormat("application/vnd.toonz.currentscene")) {
    TFilePath scenePath;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene->isUntitled()) {
      bool ok;
      QString sceneName =
          QInputDialog::getText(this, tr("Save Scene"), tr("Scene name:"),
                                QLineEdit::Normal, QString(), &ok);
      if (!ok || sceneName.isEmpty()) return false;
      scenePath = folderPath + sceneName.toStdWString();
    } else
      scenePath = folderPath + scene->getSceneName();
    return IoCmd::saveScene(scenePath, false);
  } else if (mimeData->hasFormat("application/vnd.toonz.drawings")) {
    TFilmstripSelection *s =
        dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());
    if (!s) return false;
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl) return false;

    std::wstring levelName = sl->getName();
    folderPath +=
        TFilePath(levelName + ::to_wstring(sl->getPath().getDottedType()));
    if (TSystem::doesExistFileOrLevel(folderPath)) {
      QString question =
          tr("Level %1 already exists\nDo you want to duplicate it?")
              .arg(toQString(folderPath));
      int ret =
          DVGui::MsgBox(question, tr("Duplicate"), tr("Don't Duplicate"), 0);
      if (ret == 2 || ret == 0) return false;
      TFilePath path = folderPath;
      NameBuilder *nameBuilder =
          NameBuilder::getBuilder(::to_wstring(path.getName()));
      do levelName = nameBuilder->getNext();
      while (TSystem::doesExistFileOrLevel(path.withName(levelName)));
      folderPath = path.withName(levelName);
    }
    assert(!TSystem::doesExistFileOrLevel(folderPath));

    TXshSimpleLevel *newSl = new TXshSimpleLevel();
    newSl->setType(sl->getType());
    newSl->clonePropertiesFrom(sl);
    newSl->setName(levelName);
    newSl->setPalette(sl->getPalette());
    newSl->setScene(sl->getScene());
    std::set<TFrameId> frames = s->getSelectedFids();
    for (const TFrameId &fid : frames) {
      newSl->setFrame(fid, sl->getFrame(fid, false));
    }

    IoCmd::saveLevel(folderPath, newSl, false);
    refreshFolder(folderPath.getParentDir());
    return true;
  } else if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
      TFilePath srcFp(url.toLocalFile().toStdWString());
      TFilePath dstFp = srcFp.withParentDir(folderPath);
      if (dstFp != srcFp) {
        if (!TSystem::copyFileOrLevel(dstFp, srcFp))
          DVGui::error(tr("There was an error copying %1 to %2")
                           .arg(toQString(srcFp))
                           .arg(toQString(dstFp)));
      }
    }
    refreshFolder(folderPath);
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------------------

void FileBrowser::loadResources() {
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;

  std::vector<TFilePath> filePaths;
  fs->getSelectedFiles(filePaths);
  if (filePaths.empty()) return;

  IoCmd::LoadResourceArguments args;
  args.resourceDatas.assign(filePaths.begin(), filePaths.end());
  IoCmd::loadResources(args);
}

//-----------------------------------------------------------------------------

RenameAsToonzPopup::RenameAsToonzPopup(const QString &name, int frames,
                                       bool isFolder)
    : Dialog(TApp::instance()->getMainWindow(), true, true, "RenameAsToonz") {
  setWindowTitle(tr("Rename"));

  beginHLayout();

  QLabel *lbl;
  if (frames == -1) {
    if (isFolder)
      lbl = new QLabel(tr("Renaming Folder ") + name, this);
    else
      lbl = new QLabel(tr("Renaming File ") + name, this);
  } else
    lbl = new QLabel(tr("Creating an animation level of %1 frames").arg(frames),
                     this);
  lbl->setFixedHeight(20);
  lbl->setObjectName("TitleTxtLabel");

  m_name = new LineEdit(frames == -1 ? QString() : name, this);
  m_name->setFixedHeight(20);

  m_overwrite = new QCheckBox(tr("Delete Original Files"), this);
  m_overwrite->setFixedHeight(20);

  QFormLayout *formLayout  = new QFormLayout(this);
  QHBoxLayout *labelLayout = new QHBoxLayout;
  labelLayout->addStretch();
  labelLayout->addWidget(lbl);
  labelLayout->addStretch();

  formLayout->addRow(labelLayout);
  formLayout->addRow((isFolder) ? tr("Folder Name:") : tr("Level Name:"),
                     m_name);
  if (!isFolder) formLayout->addRow(m_overwrite);

  addLayout(formLayout);
  endHLayout();

  m_okBtn = new QPushButton(tr("Rename"), this);
  m_okBtn->setDefault(true);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);

  connect(m_okBtn, &QPushButton::clicked, this, &RenameAsToonzPopup::onOk);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  addButtonBarWidget(m_okBtn, m_cancelBtn);
}

//-----------------------------------------------------------------------------

void RenameAsToonzPopup::onOk() {
  if (!isValidFileName(m_name->text())) {
    DVGui::error(
        tr("The file name cannot be empty or contain any of the following "
           "characters:(new line)  \\ / : * ? \"  |"));
    return;
  }
  if (isReservedFileName_message(m_name->text())) return;
  accept();
}

//-----------------------------------------------------------------------------

namespace {

bool parsePathName(const QString &fullpath, QString &parentPath, QString &name,
                   QString &format) {
  int index = fullpath.lastIndexOf('\\');
  if (index == -1) index = fullpath.lastIndexOf('/');

  QString filename;

  if (index != -1) {
    parentPath = fullpath.left(index + 1);
    filename   = fullpath.right(fullpath.size() - index - 1);
  } else {
    parentPath = "";
    filename   = fullpath;
  }

  index = filename.lastIndexOf('.');

  if (index <= 0) return false;

  format = filename.right(filename.size() - index - 1);
  if (format.isEmpty()) return false;

  --index;
  if (!filename.at(index).isDigit()) return false;

  while (index >= 0 && filename.at(index).isDigit()) --index;

  if (index < 0) return false;

  name = filename.left(index + 1);

  return true;
}

//---------------------------------------------------------

void getLevelFiles(const QString &parentPath, const QString &name,
                   const QString &format, QStringList &pathIn) {
  QString filter = "*." + format;
  QDir dir(parentPath, filter);
  QStringList list = dir.entryList();

  for (const QString &item : list) {
    QString dummy, dummy1, itemName;
    if (!parsePathName(item, dummy, itemName, dummy1) || name != itemName)
      continue;
    pathIn.push_back(item);
  }
}

//---------------------------------------------------------

QString getFrame(const QString &filename) {
  int index = filename.lastIndexOf('.');

  if (index <= 0) return QString();

  --index;
  if (!filename.at(index).isDigit()) return QString();

  int to = index, from = index;
  while (from >= 0 && filename.at(from).isDigit()) --from;

  if (from < 0) return QString();

  char padStr[5] = {0};
  QString number = filename.mid(from + 1, to - from);
  for (int i = 0; i < 4 - number.size(); ++i) padStr[i] = '0';
  for (int i = 0; i < number.size(); ++i)
    padStr[4 - number.size() + i] = number.at(i).toLatin1();
  return QString(padStr);
}

//-----------------------------------------------------------

void renameSingleFileOrToonzLevel(const QString &fullpath) {
  TFilePath fpin(fullpath.toStdString());

  RenameAsToonzPopup popup(
      QString::fromStdWString(fpin.withoutParentDir().getWideString()));
  if (popup.exec() != QDialog::Accepted) return;

  std::string name = popup.getName().toStdString();

  if (name == fpin.getName()) {
    DVGui::error(
        QObject::tr("The specified name is already assigned to the %1 file.")
            .arg(fullpath));
    return;
  }

  if (popup.doOverwrite())
    TSystem::renameFileOrLevel(fpin.withName(name), fpin, true);
  else {
    if (TSystem::doesExistFileOrLevel(fpin.withName(name))) {
      DVGui::error(
          QObject::tr("The specified name is already assigned to the %1 file.")
              .arg(fpin.withName(name).getQString()));
      return;
    } else
      TSystem::copyFileOrLevel(fpin.withName(name), fpin);
  }
}

//----------------------------------------------------------

void doRenameAsToonzLevel(const QString &fullpath) {
  QString parentPath, name, format;

  if (!parsePathName(fullpath, parentPath, name, format)) {
    renameSingleFileOrToonzLevel(fullpath);
    return;
  }

  QStringList pathIn;
  getLevelFiles(parentPath, name, format, pathIn);

  if (pathIn.empty()) return;

  while (name.endsWith('_') || name.endsWith('.') || name.endsWith(' '))
    name.chop(1);

  RenameAsToonzPopup popup(name, pathIn.size());
  if (popup.exec() != QDialog::Accepted) return;

  name = popup.getName();

  QString levelOutStr = parentPath + "/" + name + ".." + format;
  TFilePath levelOut(levelOutStr.toStdWString());
  if (TSystem::doesExistFileOrLevel(levelOut)) {
    QApplication::restoreOverrideCursor();
    int ret = DVGui::MsgBox(
        QObject::tr("Warning: level %1 already exists; overwrite?")
            .arg(toQString(levelOut)),
        QObject::tr("Yes"), QObject::tr("No"), 1);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (ret == 2 || ret == 0) return;
    TSystem::removeFileOrLevel(levelOut);
  }

  for (const QString &fname : pathIn) {
    QString padStr = getFrame(fname);
    if (padStr.isEmpty()) continue;
    QString pathOut = parentPath + "/" + name + "." + padStr + "." + format;

    if (popup.doOverwrite()) {
      if (!QFile::rename(parentPath + "/" + fname, pathOut)) {
        QString tmp(parentPath + "/" + fname);
        DVGui::error(
            QObject::tr("It is not possible to rename the %1 file.").arg(tmp));
        return;
      }
    } else if (!QFile::copy(parentPath + "/" + fname, pathOut)) {
      QString tmp(parentPath + "/" + fname);
      DVGui::error(
          QObject::tr("It is not possible to copy the %1 file.").arg(tmp));
      return;
    }
  }
}

}  // namespace

//-------------------------------------------------------------------------------

void FileBrowser::renameAsToonzLevel() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.size() != 1) return;

  doRenameAsToonzLevel(QString::fromStdWString(filePaths[0].getWideString()));

  QApplication::restoreOverrideCursor();

  FileBrowser::refreshFolder(filePaths[0].getParentDir());
}

//-------------------------------------------------------------------------------

void FileBrowser::renameFolder() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.size() != 1) return;

  TFilePath srcPath = filePaths[0];

  RenameAsToonzPopup popup(srcPath.withoutParentDir().getQString(), -1, true);
  if (popup.exec() != QDialog::Accepted) return;
  std::string name = popup.getName().toStdString();
  if (name == srcPath.getName()) {
    DVGui::error(
        QObject::tr("The specified name is already assigned to the folder."));
    return;
  }

  TFilePath dstPath = srcPath.getParentDir() + TFilePath(popup.getName());

  try {
    TSystem::renameFile(dstPath, srcPath);
  } catch (...) {
    return;
  }

  QApplication::restoreOverrideCursor();
  refreshFolder(srcPath.getParentDir());
}

#ifdef LEVO

void FileBrowser::convertToUnpaintedTlv() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);

  QStringList sl;
  sl << "Apply Autoclose                        "
     << "Don't Apply Autoclose                          ";
  bool ok;
  QString autoclose = QInputDialog::getItem(
      this, tr("Convert To Unpainted Tlv"), "", sl, 0, false, &ok);
  if (!ok) return;

  QApplication::setOverrideCursor(Qt::WaitCursor);

  int i, totFrames = 0;
  std::vector<Convert2Tlv *> converters;
  for (i = 0; i < filePaths.size(); i++) {
    Convert2Tlv *converter =
        new Convert2Tlv(filePaths[i], TFilePath(), TFilePath(), -1, -1,
                        autoclose == sl.at(0), TFilePath(), 0, 0, 0);

    if (TSystem::doesExistFileOrLevel(converter->m_levelOut)) {
      QApplication::restoreOverrideCursor();
      int ret = DVGui::MsgBox(tr("Warning: level %1 already exists; overwrite?")
                                  .arg(toQString(converter->m_levelOut)),
                              tr("Yes"), tr("No"), 1);
      QApplication::setOverrideCursor(Qt::WaitCursor);
      if (ret == 2 || ret == 0) {
        delete converter;
        continue;
      }
      TSystem::removeFileOrLevel(converter->m_levelOut);
    }

    totFrames += converter->getFramesToConvertCount();
    converters.push_back(converter);
  }

  if (converters.empty()) {
    QApplication::restoreOverrideCursor();
    return;
  }

  ProgressDialog pb("", "Cancel", 0, totFrames);
  int j, l, k = 0;
  for (i = 0; i < converters.size(); i++) {
    std::string errorMessage;
    if (!converters[i]->init(errorMessage)) {
      converters[i]->abort();
      DVGui::error(QString::fromStdString(errorMessage));
      delete converters[i];
      converters[i] = 0;
      continue;
    }

    int count = converters[i]->getFramesToConvertCount();

    pb.setLabelText("Generating level " + toQString(converters[i]->m_levelOut));
    pb.show();

    for (j = 0; j < count; j++) {
      std::string errorMessage = "";
      if (!converters[i]->convertNext(errorMessage) || pb.wasCanceled()) {
        for (l = i; l < converters.size(); l++) {
          converters[l]->abort();
          delete converters[i];
          converters[i] = 0;
        }
        if (errorMessage != "")
          DVGui::error(QString::fromStdString(errorMessage));
        QApplication::restoreOverrideCursor();
        FileBrowser::refreshFolder(filePaths[0].getParentDir());
        return;
      }
      pb.setValue(++k);
    }
    TFilePath levelOut(converters[i]->m_levelOut);
    delete converters[i];
    IconGenerator::instance()->invalidate(levelOut);

    converters[i] = 0;
  }

  QApplication::restoreOverrideCursor();
  pb.hide();
  DVGui::info(tr("Done: All Levels  converted to TLV Format"));

  FileBrowser::refreshFolder(filePaths[0].getParentDir());
}

//-----------------------------------------------------------------------------

void FileBrowser::convertToPaintedTlv() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);

  if (filePaths.size() != 2) return;

  QStringList sl;
  sl << "Apply Autoclose                      "
     << "Don't Apply Autoclose                        ";
  bool ok;
  QString autoclose = QInputDialog::getItem(this, tr("Convert To Painted Tlv"),
                                            "", sl, 0, false, &ok);
  if (!ok) return;

  QApplication::setOverrideCursor(Qt::WaitCursor);

  Convert2Tlv *converter =
      new Convert2Tlv(filePaths[0], filePaths[1], TFilePath(), -1, -1,
                      autoclose == sl.at(0), TFilePath(), 0, 0, 0);

  if (TSystem::doesExistFileOrLevel(converter->m_levelOut)) {
    QApplication::restoreOverrideCursor();
    int ret = DVGui::MsgBox(tr("Warning: level %1 already exists; overwrite?")
                                .arg(toQString(converter->m_levelOut)),
                            tr("Yes"), tr("No"), 1);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (ret == 2 || ret == 0) {
      QApplication::restoreOverrideCursor();
      return;
    }
    TSystem::removeFileOrLevel(converter->m_levelOut);
  }

  std::string errorMessage;
  if (!converter->init(errorMessage)) {
    converter->abort();
    delete converter;
    DVGui::error(QString::fromStdString(errorMessage));
    QApplication::restoreOverrideCursor();
    return;
  }
  int count = converter->getFramesToConvertCount();

  ProgressDialog pb("Generating level " + toQString(converter->m_levelOut),
                    "Cancel", 0, count);
  pb.show();

  for (int i = 0; i < count; i++) {
    errorMessage = "";
    if (!converter->convertNext(errorMessage) || pb.wasCanceled()) {
      converter->abort();
      delete converter;
      if (errorMessage != "")
        DVGui::error(QString::fromStdString(errorMessage));
      QApplication::restoreOverrideCursor();
      FileBrowser::refreshFolder(filePaths[0].getParentDir());
      return;
    }

    pb.setValue(i + 1);
  }

  TFilePath levelOut(converter->m_levelOut);
  delete converter;
  IconGenerator::instance()->invalidate(levelOut);

  QApplication::restoreOverrideCursor();
  pb.hide();
  DVGui::info(tr("Done: 2 Levels  converted to TLV Format"));

  fs->selectNone();
  FileBrowser::refreshFolder(filePaths[0].getParentDir());
}
#endif

//-----------------------------------------------------------------------------

bool FileBrowser::getInfoPanelFile(TFilePath &path) const {
  const FileSelection *fs = dynamic_cast<const FileSelection *>(
      m_itemViewer->getPanel()->getSelection());
  if (!fs || fs->isEmpty()) return false;

  for (int idx : fs->getSelectedIndices()) {
    if (idx < 0 || idx >= (int)m_items.size()) continue;
    if (m_items[idx].m_isFolder || m_items[idx].m_name == QStringLiteral(".."))
      continue;
    path = m_items[idx].m_path;
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void FileBrowser::applyInfoPanelSize() {
  if (!m_itemsSplitter || !m_infoPanelVisible) return;
  QList<int> sizes = m_itemsSplitter->sizes();
  if (sizes.size() != 2) return;
  const int total = sizes[0] + sizes[1];
  if (total < 80) return;
  int infoWidth = (int)BrowserInfoPanelWidth;
  if (infoWidth < 140) infoWidth = 220;
  infoWidth = qMin(infoWidth, qMax(140, total - 80));
  sizes[0]  = total - infoWidth;
  sizes[1]  = infoWidth;
  m_itemsSplitter->setSizes(sizes);
}

//-----------------------------------------------------------------------------

void FileBrowser::setInfoPanelVisible(bool visible) {
  if (!m_infoScrollArea || !m_itemsSplitter) return;
  if (m_infoPanelVisible == visible) return;

  m_infoPanelVisible = visible;
  BrowserInfoPanelVisible = visible ? 1 : 0;
  m_infoScrollArea->setVisible(visible);

  QList<int> sizes = m_itemsSplitter->sizes();
  if (sizes.size() == 2) {
    const int total = sizes[0] + sizes[1];
    if (visible) {
      applyInfoPanelSize();
      if (total < 80)
        QTimer::singleShot(0, this, [this]() { applyInfoPanelSize(); });
    } else if (sizes[1] > 0) {
      BrowserInfoPanelWidth = sizes[1];
      m_itemsSplitter->setSizes({total, 0});
    }
  }

  if (m_buttonBar) {
    m_buttonBar->setInfoPanelChecked(visible);
    m_buttonBar->setInfoPanelEnabled(true);
  }
  if (visible) {
    QTimer::singleShot(0, this, &FileBrowser::refreshInfoPanelFromSelection);
  }
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void FileBrowser::onItemsSplitterMoved(int, int) {
  if (!m_itemsSplitter || !m_infoPanelVisible) return;
  QList<int> sizes = m_itemsSplitter->sizes();
  if (sizes.size() == 2 && sizes[1] > 0) BrowserInfoPanelWidth = sizes[1];

  // Re-render if the panel is wider than the cached size.
  if (m_infoThumbnail && m_infoCurrentPath != TFilePath() &&
      !m_infoThumbReqSize.isEmpty()) {
    const int wanted =
        (int)(infoThumbPanelWidth() * m_infoThumbnail->devicePixelRatioF());
    if (wanted > m_infoThumbReqSize.width()) {
      updateInfoThumbnail(m_infoCurrentPath);
    } else {
      // Re-fit from cache while dragging the splitter.
      QPixmap src = IconGenerator::instance()->peekSizedIcon(
          m_infoCurrentPath, TDimension(m_infoThumbReqSize.width(),
                                        m_infoThumbReqSize.height()));
      if (!src.isNull()) setInfoThumbnailPixmap(src);
    }
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::onInfoPanelActionTriggered(bool on) {
  setInfoPanelVisible(on);
}

//-----------------------------------------------------------------------------

void FileBrowser::onInfoPanelContextMenu(const QPoint &pos) {
  QWidget *w = qobject_cast<QWidget *>(sender());
  if (!w || !m_infoPanelVisible) return;

  QMenu menu(this);
  QAction *hideAct = menu.addAction(tr("Hide"));
  if (menu.exec(w->mapToGlobal(pos)) == hideAct) setInfoPanelVisible(false);
}

//-----------------------------------------------------------------------------

int FileBrowser::infoThumbPanelWidth() const {
  int panelW =
      m_infoScrollArea ? m_infoScrollArea->viewport()->width() - 8 : 160;
  return qMax(60, panelW);
}

//-----------------------------------------------------------------------------

int FileBrowser::infoThumbBgMode() const {
  DvItemViewerPanel *panel =
      m_itemViewer ? m_itemViewer->getPanel() : nullptr;
  if (!panel || !panel->isAdvancedDisplay()) return 0;
  int mode = (int)panel->getThumbnailBgMode();
  if (m_infoCurrentPath != TFilePath() &&
      supportsBrowserThumbnailCustomization(m_infoCurrentPath)) {
    const int ov =
        BrowserFileSettings::instance()->thumbnailBgOverride(m_infoCurrentPath);
    if (ov >= 0) mode = ov;
  }
  return mode;
}

//-----------------------------------------------------------------------------

void FileBrowser::updateInfoThumbnail(const TFilePath &fp) {
  if (!m_infoThumbnail) return;
  if (fp == TFilePath()) {
    m_infoCurrentPath  = fp;
    m_infoThumbReqSize = QSize();
    m_infoThumbnail->setPixmap(QPixmap());
    m_infoThumbnail->setFixedHeight(0);
    return;
  }

  const QPixmap *curPx = m_infoThumbnail->pixmap();
  const bool hasPixmap = curPx && !curPx->isNull();
  m_infoCurrentPath    = fp;

  const double dpr = m_infoThumbnail->devicePixelRatioF();

  // Snap to coarse buckets: the cache key includes the render size.
  const int wanted = (int)(infoThumbPanelWidth() * dpr);
  const int bucket = 64;
  const int req    = qMax(128, ((wanted + bucket - 1) / bucket) * bucket);
  m_infoThumbReqSize = QSize(req, req);

  const TFrameId fid =
      m_infoViewer ? m_infoViewer->currentFrameId() : TFrameId::NO_FRAME;
  const TDimension dim(req, req);
  const int bgMode = infoThumbBgMode();

  QPixmap px = IconGenerator::instance()->peekSizedIcon(fp, dim, fid, bgMode);
  if (px.isNull())
    px = IconGenerator::instance()->getSizedIcon(fp, dim, fid, bgMode);
  if (px.isNull()) px = peekAnyBgIcon(fp, dim, fid, bgMode);
  if (px.isNull() && !hasPixmap)
    px = IconGenerator::instance()->getIcon(fp, fid);
  if (px.isNull()) return;

  setInfoThumbnailPixmap(px);
}

//-----------------------------------------------------------------------------

void FileBrowser::setInfoThumbnailPixmap(const QPixmap &px) {
  if (!m_infoThumbnail || px.isNull()) return;
  const int panelW = infoThumbPanelWidth();
  const double dpr = m_infoThumbnail->devicePixelRatioF();
  QPixmap scaled   = px.scaled(QSize(panelW, qMin(panelW, 200)) * dpr,
                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
  scaled.setDevicePixelRatio(dpr);
  m_infoThumbnail->setPixmap(scaled);
  m_infoThumbnail->setFixedHeight((int)(scaled.height() / dpr) + 4);
}

//-----------------------------------------------------------------------------

void FileBrowser::onIconGenerated() {
  if (!m_infoPanelVisible || !m_infoThumbnail || !m_infoThumbVisible) return;
  if (m_infoCurrentPath == TFilePath()) return;
  if (m_infoThumbReqSize.isEmpty()) return;

  const TFrameId fid =
      m_infoViewer ? m_infoViewer->currentFrameId() : TFrameId::NO_FRAME;
  const TDimension dim(m_infoThumbReqSize.width(), m_infoThumbReqSize.height());
  const int bgMode = infoThumbBgMode();
  QPixmap px =
      IconGenerator::instance()->peekSizedIcon(m_infoCurrentPath, dim, fid, bgMode);
  if (px.isNull()) return;
  if (px.isNull()) return;

  setInfoThumbnailPixmap(px);
}

void FileBrowser::refreshInfoPanelFromSelection() {
  if (!m_infoPanelVisible || !m_infoViewer) return;

  TFilePath fp;
  if (!getInfoPanelFile(fp)) return;
  m_infoViewer->setItem(TLevelP(), nullptr, fp);
  updateInfoThumbnail(fp);
}

//-----------------------------------------------------------------------------

void FileBrowser::onSelectedItems(const std::set<int> &indexes) {
  std::set<TFilePath> filePaths;
  std::list<std::vector<TFrameId>> frameIDs;

  if (indexes.empty()) {
    m_persistedSelection.clear();
    emit filePathsSelected(filePaths, frameIDs);
    if (m_buttonBar) m_buttonBar->setInfoPanelEnabled(true);
    if (m_infoPanelVisible) refreshInfoPanelFromSelection();
    return;
  }

  size_t itemsSize = m_items.size();
  m_persistedSelection.clear();
  m_persistedSelection.reserve(indexes.size());
  bool hasInfoTarget = false;
  for (int idx : indexes) {
    if (idx < 0 || static_cast<size_t>(idx) >= itemsSize) continue;

    filePaths.insert(m_items[idx].m_path);
    frameIDs.push_back(m_items[idx].m_frameIds);
    m_persistedSelection.push_back(m_items[idx].m_path);
    if (!m_items[idx].m_isFolder && m_items[idx].m_name != QStringLiteral(".."))
      hasInfoTarget = true;
  }

  if (m_buttonBar) m_buttonBar->setInfoPanelEnabled(true);
  if (m_infoPanelVisible) refreshInfoPanelFromSelection();

  emit filePathsSelected(filePaths, frameIDs);
}

//-----------------------------------------------------------------------------

void FileBrowser::onClickedItem(int index) {
  if (0 <= index && index < (int)m_items.size()) {
    TFilePath fp = m_items[index].m_path;
    if (m_items[index].m_isFolder) {
      if (!Preferences::instance()->isFileBrowserFolderDoubleClick()) {
        setFolder(fp, true);
        QModelIndex idx = m_folderTreeView->currentIndex();
        if (idx.isValid()) m_folderTreeView->scrollTo(idx);
      }
    } else
      emit filePathClicked(fp);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::onDoubleClickedItem(int index) {
  if (0 <= index && index < (int)m_items.size()) {
    TFilePath fp = m_items[index].m_path;
    if (m_items[index].m_isFolder) {
      setFolder(fp, true);
      QModelIndex idx = m_folderTreeView->currentIndex();
      if (idx.isValid()) m_folderTreeView->scrollTo(idx);
    } else
      emit filePathDoubleClicked(fp);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshFolder(const TFilePath &folderPath) {
  for (FileBrowser *browser : activeBrowsers) {
    DvDirModel::instance()->refreshFolder(folderPath);
    if (browser->getFolder() == folderPath) {
      browser->setFolder(folderPath, false, true);
    }
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::updateItemViewerPanel() {
  for (FileBrowser *browser : activeBrowsers) {
    browser->m_itemViewer->getPanel()->update();
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::getExpandedFolders(DvDirModelNode *node,
                                     QList<DvDirModelNode *> &expandedNodes) {
  if (!node) return;
  QModelIndex newIndex = DvDirModel::instance()->getIndexByNode(node);
  if (!m_folderTreeView->isExpanded(newIndex)) return;
  expandedNodes.push_back(node);

  for (int i = 0; i < node->getChildCount(); ++i)
    getExpandedFolders(node->getChild(i), expandedNodes);
}

//-----------------------------------------------------------------------------

void FileBrowser::refresh() {
  TFilePath originalFolder(m_folder);

  int dx                   = m_folderTreeView->verticalScrollBar()->value();
  DvDirModelNode *rootNode = DvDirModel::instance()->getNode(QModelIndex());

  QModelIndex index = DvDirModel::instance()->getIndexByNode(rootNode);

  bool vcEnabled = m_folderTreeView->refreshVersionControlEnabled();

  m_folderTreeView->setRefreshVersionControlEnabled(false);
  DvDirModel::instance()->refreshFolderChild(index);
  m_folderTreeView->setRefreshVersionControlEnabled(vcEnabled);

  QList<DvDirModelNode *> expandedNodes;
  for (int i = 0; i < rootNode->getChildCount(); ++i)
    getExpandedFolders(rootNode->getChild(i), expandedNodes);

  for (DvDirModelNode *node : expandedNodes) {
    if (!node || !node->hasChildren()) continue;
    QModelIndex ind = DvDirModel::instance()->getIndexByNode(node);
    if (!ind.isValid()) continue;
    m_folderTreeView->expand(ind);
  }
  m_folderTreeView->verticalScrollBar()->setValue(dx);

  setFolder(originalFolder, false, true);
}

//-----------------------------------------------------------------------------

void FileBrowser::folderUp() {
  QModelIndex index = m_folderTreeView->currentIndex();
  if (!index.isValid() || !index.parent().isValid()) {
    // cannot go up tree view, so try going to parent directory
    TFilePath parentFp = m_folder.getParentDir();
    if (parentFp != TFilePath("") && parentFp != m_folder) {
      setFolder(parentFp, true);
    }
    return;
  }
  m_folderTreeView->setCurrentIndex(index.parent());
  m_folderTreeView->scrollTo(index.parent());
}

//-----------------------------------------------------------------------------

void FileBrowser::newFolder() {
  TFilePath parentFolder = getFolder();
  if (parentFolder == TFilePath() || !TFileStatus(parentFolder).isDirectory())
    return;
  QString tempName(tr("New Folder"));
  std::wstring folderName = tempName.toStdWString();
  TFilePath folderPath    = parentFolder + folderName;
  int i                   = 1;
  while (TFileStatus(folderPath).doesExist())
    folderPath = parentFolder + (folderName + L" " + std::to_wstring(++i));

  try {
    TSystem::mkDir(folderPath);
  } catch (...) {
    DVGui::error(tr("It is not possible to create the %1 folder.")
                     .arg(toQString(folderPath)));
    return;
  }

  DvDirModel *model = DvDirModel::instance();

  QModelIndex parentFolderIndex = m_folderTreeView->currentIndex();
  model->refresh(parentFolderIndex);
  m_folderTreeView->expand(parentFolderIndex);

  std::wstring newFolderName = folderPath.getWideName();
  QModelIndex newFolderIndex =
      model->childByName(parentFolderIndex, newFolderName);
  if (newFolderIndex.isValid()) {
    m_folderTreeView->setCurrentIndex(newFolderIndex);
    m_folderTreeView->scrollTo(newFolderIndex);
    m_folderTreeView->QTreeView::edit(newFolderIndex);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::showEvent(QShowEvent *) {
  activeBrowsers.insert(this);

  // Snapshot selection before the folder force-refresh.
  storePersistedSelection();

  // refresh
  if (getFolder() != TFilePath())
    setFolder(getFolder(), false, true);
  else if (!getDayDateString().empty())
    setHistoryDay(getDayDateString());
  m_folderTreeView->scrollTo(m_folderTreeView->currentIndex());

  // Restore after folder refresh remaps indices.
  QTimer::singleShot(0, this, [this]() { restorePersistedSelection(); });

  // Refresh SVN
  DvDirVersionControlNode *vcNode = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (vcNode) m_folderTreeView->refreshVersionControl(vcNode);

  if (m_buttonBar) m_buttonBar->refreshProjectFolderShortcuts();

  if (m_infoPanelVisible)
    QTimer::singleShot(0, this, [this]() { applyInfoPanelSize(); });
}

//-----------------------------------------------------------------------------

void FileBrowser::resizeEvent(QResizeEvent *e) {
  QFrame::resizeEvent(e);
  if (m_infoPanelVisible) applyInfoPanelSize();
}

//-----------------------------------------------------------------------------

void FileBrowser::hideEvent(QHideEvent *) {
  storePersistedSelection();
  activeBrowsers.erase(this);
  m_itemViewer->getPanel()->getItemViewPlayDelegate()->resetPlayWidget();
}

//-----------------------------------------------------------------------------

void FileBrowser::makeCurrentProjectVisible() {}

//-----------------------------------------------------------------------------

void FileBrowser::enableGlobalSelection(bool enabled) {
  m_folderTreeView->enableGlobalSelection(enabled);
  m_itemViewer->enableGlobalSelection(enabled);
}

//-----------------------------------------------------------------------------

void FileBrowser::selectNone() { m_itemViewer->selectNone(); }

//-----------------------------------------------------------------------------

void FileBrowser::enableDoubleClickToOpenScenes() {
  connect(this, &FileBrowser::filePathDoubleClicked, this,
          &FileBrowser::tryToOpenScene);
}

//-----------------------------------------------------------------------------

void FileBrowser::tryToOpenScene(const TFilePath &filePath) {
  if (filePath.getType() == "tnz") {
    IoCmd::loadScene(filePath);
  }
}

//=============================================================================
// FCData methods
//-----------------------------------------------------------------------------

FCData::FCData(const QDateTime &date)
    : m_date(date), m_frameCount(0), m_underProgress(true), m_retryCount(1) {}

//=============================================================================
// FrameCountReader methods
//-----------------------------------------------------------------------------

FrameCountReader::FrameCountReader() : m_executor() {
  m_executor.setMaxActiveTasks(2);
}

//-----------------------------------------------------------------------------

FrameCountReader::~FrameCountReader() = default;

//-----------------------------------------------------------------------------

int FrameCountReader::getFrameCount(const TFilePath &fp) {
  QDateTime modifiedDate =
      QFileInfo(QString::fromStdWString(fp.getWideString())).lastModified();

  {
    QMutexLocker locker(&frameCountMapMutex);
    auto it = frameCountMap.find(fp);

    if (it != frameCountMap.end()) {
      if (it->second.m_frameCount > 0 && it->second.m_date == modifiedDate) {
        return it->second.m_frameCount;
      }
      if ((modifiedDate == it->second.m_date) &&
          (it->second.m_underProgress || it->second.m_retryCount < 0)) {
        return -1;
      }
    } else {
      frameCountMap[fp] = FCData(modifiedDate);
    }
  }

  // We have to calculate the frame count; create a task and submit it.
  auto *task = new FrameCountTask(fp, modifiedDate);
  connect(task, &FrameCountTask::finished, this,
          &FrameCountReader::calculatedFrameCount);
  connect(task, &FrameCountTask::exception, this,
          &FrameCountReader::calculatedFrameCount);

  m_executor.addTask(task);
  return -1;
}

//-----------------------------------------------------------------------------

void FrameCountReader::stopReading() { m_executor.cancelAll(); }

//=============================================================================
// FrameCountTask methods
//-----------------------------------------------------------------------------

FrameCountTask::FrameCountTask(const TFilePath &path,
                               const QDateTime &modifiedDate)
    : m_path(path), m_modifiedDate(modifiedDate), m_started(false) {
  connect(this, &FrameCountTask::started, this, &FrameCountTask::onStarted);
  connect(this, &FrameCountTask::canceled, this, &FrameCountTask::onCanceled);
}

//-----------------------------------------------------------------------------

FrameCountTask::~FrameCountTask() = default;

//-----------------------------------------------------------------------------

void FrameCountTask::run() {
  TLevelReaderP lr(m_path);
  int frameCount = lr->loadInfo()->getFrameCount();

  QMutexLocker fCMapMutex(&frameCountMapMutex);

  auto it = frameCountMap.find(m_path);
  if (it == frameCountMap.end()) return;

  // Memorize the found frameCount into the frameCountMap
  if (frameCount > 0) {
    it->second.m_frameCount = frameCount;
    it->second.m_date       = m_modifiedDate;
  } else {
    // Seems that tlv reads sometimes may fail, returning invalid frame counts
    // (typically 0). However, if no exception was thrown, we try to recover it
    it->second.m_underProgress = false;
    it->second.m_retryCount--;
  }
}

//-----------------------------------------------------------------------------

QThread::Priority FrameCountTask::runningPriority() {
  return QThread::LowPriority;
}

//-----------------------------------------------------------------------------

void FrameCountTask::onStarted(TThread::RunnableP) { m_started = true; }

//-----------------------------------------------------------------------------

void FrameCountTask::onCanceled(TThread::RunnableP) {
  if (!m_started) {
    QMutexLocker fCMapMutex(&frameCountMapMutex);
    frameCountMap.erase(m_path);
  }
}

//=============================================================================

OpenFloatingPanel openBrowserPane(MI_OpenFileBrowser, "Browser",
                                  QObject::tr("File Browser"));
