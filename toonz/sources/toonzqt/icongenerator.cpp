

// TnzCore includes
#include "tfilepath.h"
#include "tfiletype.h"
#include "tstream.h"
#include "tsystem.h"
#include "timagecache.h"
#include "tpixelutils.h"
#include "tropcm.h"
#include "timageinfo.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tgl.h"
#include "tvectorrenderdata.h"
#include "tstroke.h"
#include "tthreadmessage.h"
#include "tpalette.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tmeshimage.h"

// TnzExt includes
#include "ext/meshutils.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/preferences.h"
#include "toonz/sceneresources.h"
#include "toonz/stage2.h"
#include "trop.h"

// TnzQt includes
#include "toonzqt/gutil.h"

#include "toonzqt/icongenerator.h"

#include <QCoreApplication>
#include <vector>

//=============================================================================

//===================================
//
//    Local namespace
//
//-----------------------------------

namespace {
const TDimension IconSize(80, 60);
TDimension FilmstripIconSize(0, 0);

// Matches DvItemViewerPanel::ThumbnailBgMode::BgAuto (value 4).
constexpr int kBrowserBgAuto = 4;

IconGenerator::Settings browserFileIconSettings(int browserBgMode) {
  IconGenerator::Settings s;
  if (browserBgMode == kBrowserBgAuto) return s;  // Auto: official opaque icons
  // Forced bg: transparent letterbox; UI draws the fill.
  s.m_transparentBg = true;
  s.m_blackBgCheck  = (browserBgMode == 2);  // BgBlack
  return s;
}

// Access name-based storage
std::set<std::string> iconsMap;
typedef std::set<std::string>::iterator IconIterator;

//-----------------------------------------------------------------------------

// Returns true if the image request was already submitted.
bool getIcon(const std::string &iconName, QPixmap &pix,
             TXshSimpleLevel *simpleLevel = 0,
             TDimension standardSize      = TDimension(0, 0)) {
  IconIterator it;
  it = iconsMap.find(iconName);

  if (it != iconsMap.end()) {
    TImageP im         = TImageCache::instance()->get(iconName, false);
    TToonzImage *timgp = dynamic_cast<TToonzImage *>(im.getPointer());

    if (simpleLevel && timgp) {
      IconGenerator::Settings settings =
          IconGenerator::instance()->getSettings();

      TRaster32P icon(timgp->getSize());
      icon->clear();
      icon->fill((settings.m_blackBgCheck) ? TPixel::Black : TPixel::White);
      if (settings.m_transparencyCheck || settings.m_inkIndex != -1 ||
          settings.m_paintIndex != -1) {
        TRop::CmappedQuickputSettings s;
        s.m_globalColorScale  = TPixel32::Black;
        s.m_inksOnly          = false;
        s.m_transparencyCheck = settings.m_transparencyCheck;
        s.m_blackBgCheck      = settings.m_blackBgCheck;
        s.m_inkIndex          = settings.m_inkIndex;
        s.m_paintIndex        = settings.m_paintIndex;
        Preferences::instance()->getTranspCheckData(
            s.m_transpCheckBg, s.m_transpCheckInk, s.m_transpCheckPaint);
        s.m_inkCheckEnabled   = settings.m_inkCheckEnabled;
        s.m_ink1CheckEnabled  = settings.m_ink1CheckEnabled;
        s.m_paintCheckEnabled = settings.m_paintCheckEnabled;
        s.m_inkCheckColor     = Preferences::instance()->getInkCheckColor();
        s.m_ink1CheckColor    = Preferences::instance()->getInk1CheckColor();
        s.m_paintCheckColor   = Preferences::instance()->getPaintCheckColor();

        TRop::quickPut(icon, timgp->getRaster(), simpleLevel->getPalette(),
                       TAffine(), s);
      } else
        TRop::quickPut(icon, timgp->getRaster(), simpleLevel->getPalette(),
                       TAffine());
      pix = rasterToQPixmap(icon, false);
      return true;
    }
    TRasterImageP img = static_cast<TRasterImageP>(im);

    if (!img) {
      pix = QPixmap();
      return true;
    }
    assert(!(TRasterGR8P)img->getRaster());
    const TRaster32P ras = img->getRaster();
    // Invalid cache entry: drop it so the icon can be requested again.
    if (!ras || ras->getLx() <= 0 || ras->getLy() <= 0) {
      TImageCache::instance()->remove(iconName);
      iconsMap.erase(iconName);
      pix = QPixmap();
      return false;
    }
    bool isHighDpi = false;
    // If the icon raster obtained in higher resolution than the standard
    // icon size, it may be icon displayed in high dpi monitors.
    // In such case set the device pixel ratio to the pixmap.
    // Note that the humbnails of regular levels are standardsize even if
    // they are displayed in high dpi monitors for now.
    if (standardSize != TDimension(0, 0) &&
        ras->getSize().lx > standardSize.lx &&
        ras->getSize().ly > standardSize.ly)
      isHighDpi = true;
    pix = rasterToQPixmap(ras, false, isHighDpi);
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

void setIcon(const std::string &iconName, const TRaster32P &icon) {
  if (iconsMap.find(iconName) != iconsMap.end())
    TImageCache::instance()->add(iconName, TRasterImageP(icon), true);
}

//-----------------------------------------------------------------------------
/*! Cache icon data in TToonzImage format if ToonzImageIconRenderer generates
 * them
 */
void setIcon_TnzImg(const std::string &iconName, const TRasterCM32P &icon) {
  if (iconsMap.find(iconName) != iconsMap.end())
    TImageCache::instance()->add(
        iconName, TToonzImageP(icon, TRect(icon->getSize())), true);
}

//-----------------------------------------------------------------------------

void removeIcon(const std::string &iconName) {
  IconIterator it;
  it = iconsMap.find(iconName);
  if (it != iconsMap.end()) {
    TImageCache::instance()->remove(iconName);
  }
  iconsMap.erase(iconName);
}

//-----------------------------------------------------------------------------
/*! Remove sized cache entries (suffix "_r_WxH"). */
void removeResponsiveSizedIcons(const std::string &baseId) {
  const std::string prefix = baseId + "_r_";
  std::vector<std::string> toRemove;
  for (IconIterator it = iconsMap.lower_bound(prefix);
       it != iconsMap.end() && it->compare(0, prefix.size(), prefix) == 0;
       ++it) {
    toRemove.push_back(*it);
  }
  for (const std::string &key : toRemove) removeIcon(key);
}

//-----------------------------------------------------------------------------

bool isUnpremultiplied(const TRaster32P &r) {
  int lx = r->getLx();
  int y  = r->getLy();
  r->lock();
  while (--y >= 0) {
    TPixel32 *pix    = r->pixels(y);
    TPixel32 *endPix = pix + lx;
    while (pix < endPix) {
      if (pix->r > pix->m || pix->g > pix->m || pix->b > pix->m) {
        r->unlock();
        return true;
      }
      ++pix;
    }
  }
  r->unlock();
  return false;
}

//-----------------------------------------------------------------------------

void makeChessBackground(const TRaster32P &ras) {
  ras->lock();

  const TPixel32 gray1(230, 230, 230, 255), gray2(180, 180, 180, 255);

  int lx = ras->getLx(), ly = ras->getLy();
  for (int y = 0; y != ly; ++y) {
    TPixel32 *pix = ras->pixels(y), *lineEnd = pix + lx;

    int yCol = (y & 4);

    for (int x = 0; pix != lineEnd; ++x, ++pix)
      if (pix->m != 255) *pix = overPix((x & 4) == yCol ? gray1 : gray2, *pix);
  }

  ras->unlock();
}

//-----------------------------------------------------------------------------
/*! Ortho/viewport for \p iconSize. TLS OfflineGL may be larger than the request. */
void prepareIconGL(TOfflineGL *glContext, const TDimension &iconSize) {
  if (!glContext || iconSize.lx <= 0 || iconSize.ly <= 0) return;
  glContext->makeCurrent();
  glViewport(0, 0, iconSize.lx, iconSize.ly);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, iconSize.lx, 0, iconSize.ly);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

}  // namespace

//=============================================================================

//==========================================
//
//    Image-to-Icon conversion methods
//
//------------------------------------------

namespace {

TRaster32P convertToIcon(TVectorImageP vimage, int frame,
                         const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!vimage) return TRaster32P();

  TPalette *plt = vimage->getPalette()->clone();
  if (!plt) return TRaster32P();
  plt->setFrame(frame);

  TRectD imageBox;
  {
    QMutexLocker sl(vimage->getMutex());
    imageBox = vimage->getBBox().enlarge(.1);
  }

  TPointD imageCenter = (imageBox.getP00() + imageBox.getP11()) * 0.5;

  // HD vector thumbs: draw at 2x then downsample so strokes stay smooth.
  int ss = (iconSize.lx > 80 || iconSize.ly > 60) ? 2 : 1;
  while (ss > 1 &&
         (iconSize.lx * ss > 1024 || iconSize.ly * ss > 1024))
    --ss;
  const TDimension glSize(iconSize.lx * ss, iconSize.ly * ss);

  TOfflineGL *glContext =
      IconGenerator::instance()->getOfflineGLContext(glSize);

  const int margin = 10 * ss;
  double scx       = (glSize.lx - margin) / imageBox.getLx();
  double scy       = (glSize.ly - margin) / imageBox.getLy();
  double sc        = std::min(scx, scy);
  TPointD iconCenter(glSize.lx * 0.5, glSize.ly * 0.5);
  TAffine aff = TScale(sc).place(imageCenter, iconCenter);

  TVectorRenderData rd(aff, TRect(glSize), plt, 0, true);
  rd.m_tcheckEnabled  = settings.m_transparencyCheck;
  rd.m_blackBgEnabled = settings.m_blackBgCheck;
  rd.m_drawRegions    = !settings.m_inksOnly;
  rd.m_isIcon         = true;
  rd.m_regionAntialias = (ss > 1);

  rd.m_inkCheckEnabled   = settings.m_inkCheckEnabled;
  rd.m_ink1CheckEnabled  = settings.m_ink1CheckEnabled;
  rd.m_paintCheckEnabled = settings.m_paintCheckEnabled;
  rd.m_paintIndex        = settings.m_paintIndex;

  if (rd.m_inkCheckEnabled || rd.m_ink1CheckEnabled)
    rd.m_colorCheckIndex = settings.m_inkIndex;
  else if (rd.m_paintCheckEnabled)
    rd.m_colorCheckIndex = settings.m_paintIndex;
  else
    rd.m_colorCheckIndex = -1;

  Preferences *pref    = Preferences::instance();
  rd.m_inkCheckColor   = pref->getInkCheckColor();
  rd.m_ink1CheckColor  = pref->getInk1CheckColor();
  rd.m_paintCheckColor = pref->getPaintCheckColor();

  glContext->makeCurrent();
  if (settings.m_transparentBg)
    glContext->clear(TPixel32::Transparent);
  else
    glContext->clear(rd.m_blackBgEnabled ? TPixel::Black : TPixel32::White);
  prepareIconGL(glContext, glSize);
  glContext->draw(vimage, rd, false);

  TRaster32P ras(glSize);
  glContext->getRaster(ras);
  glContext->doneCurrent();

  delete plt;

  if (ss == 1) return ras;

  TRaster32P icon(iconSize);
  if (settings.m_transparentBg)
    icon->clear();
  else
    icon->fill(rd.m_blackBgEnabled ? TPixel::Black : TPixel32::White);
  TAffine down =
      TScale(1.0 / ss).place(ras->getCenterD(), icon->getCenterD());
  TRop::resample(icon, ras, down, TRop::Triangle);
  return icon;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TToonzImageP timage, int frame,
                         const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!timage) return TRaster32P();

  TPalette *plt = timage->getPalette();
  if (!plt) return TRaster32P();

  plt->setFrame(frame);

  TRasterCM32P rasCM32 = timage->getRaster();
  if (!rasCM32.getPointer()) return TRaster32P();

  int lx     = rasCM32->getSize().lx;
  int ly     = rasCM32->getSize().ly;
  int iconLx = iconSize.lx, iconLy = iconSize.ly;
  if (std::max(double(lx) / iconSize.lx, double(ly) / iconSize.ly) ==
      double(ly) / iconSize.ly)
    iconLx = tround((double(lx) * iconSize.ly) / ly);
  else
    iconLy = tround((double(ly) * iconSize.lx) / lx);

  TDimension iconSize2 = TDimension(iconLx, iconLy);

  TRaster32P icon(iconSize2);
  icon->clear();
  icon->fill(settings.m_blackBgCheck ? TPixel::Black : TPixel::White);

  TDimension dim = rasCM32->getSize();
  if (dim != iconSize2) {
    TRasterCM32P auxCM32(icon->getSize());
    auxCM32->clear();
    TRop::makeIcon(auxCM32, rasCM32);
    rasCM32 = auxCM32;
  }

  if (settings.m_transparencyCheck || settings.m_inksOnly ||
      settings.m_inkIndex != -1 || settings.m_paintIndex != -1) {
    TRop::CmappedQuickputSettings s;
    s.m_globalColorScale  = TPixel32::Black;
    s.m_inksOnly          = settings.m_inksOnly;
    s.m_transparencyCheck = settings.m_transparencyCheck;
    s.m_blackBgCheck      = settings.m_blackBgCheck;
    s.m_inkIndex          = settings.m_inkIndex;
    s.m_paintIndex        = settings.m_paintIndex;
    Preferences::instance()->getTranspCheckData(
        s.m_transpCheckBg, s.m_transpCheckInk, s.m_transpCheckPaint);
    s.m_inkCheckEnabled   = settings.m_inkCheckEnabled;
    s.m_ink1CheckEnabled  = settings.m_ink1CheckEnabled;
    s.m_paintCheckEnabled = settings.m_paintCheckEnabled;
    s.m_inkCheckColor     = Preferences::instance()->getInkCheckColor();
    s.m_ink1CheckColor    = Preferences::instance()->getInk1CheckColor();
    s.m_paintCheckColor   = Preferences::instance()->getPaintCheckColor();

    TRop::quickPut(icon, rasCM32, timage->getPalette(), TAffine(), s);
  } else
    TRop::quickPut(icon, rasCM32, timage->getPalette(), TAffine());

  assert(iconSize2.lx <= iconSize.lx && iconSize2.ly <= iconSize.ly);
  TRaster32P outIcon(iconSize);
  outIcon->clear();
  int dx = (outIcon->getLx() - icon->getLx()) / 2;
  int dy = (outIcon->getLy() - icon->getLy()) / 2;
  assert(dx >= 0 && dy >= 0);
  TRect box = outIcon->getBounds().enlarge(-dx, -dy);
  TRop::copy(outIcon->extract(box), icon);

  return outIcon;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TRasterImageP rimage, const TDimension &iconSize) {
  if (!rimage) return TRaster32P();
  TRasterP ras = rimage->getRaster();
  if (!(TRaster32P)ras && !(TRasterGR8P)ras) return TRaster32P();
  if (ras->getSize() == iconSize) return ras;

  TRaster32P icon(iconSize);
  icon->fill(TPixel32(235, 235, 235));

  double sx = (double)icon->getLx() / ras->getLx();
  double sy = (double)icon->getLy() / ras->getLy();
  double sc = sx < sy ? sx : sy;

  TAffine aff = TScale(sc).place(ras->getCenterD(), icon->getCenterD());
  TRop::resample(icon, ras, aff, TRop::Bilinear);
  TRop::addBackground(icon, TPixel32::White);

  return icon;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TMeshImageP mi, int frame, const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!mi) return TRaster32P();

  TOfflineGL *glContext =
      IconGenerator::instance()->getOfflineGLContext(iconSize);
  TRectD imageBox = mi->getBBox().enlarge(.1);
  TPointD imageCenter(0.5 * (imageBox.getP00() + imageBox.getP11()));

  const int margin = 10;
  double scx       = (iconSize.lx - margin) / imageBox.getLx();
  double scy       = (iconSize.ly - margin) / imageBox.getLy();
  double sc        = std::min(scx, scy);

  TPointD iconCenter(iconSize.lx * 0.5, iconSize.ly * 0.5);
  TAffine aff = TScale(sc).place(imageCenter, iconCenter);

  glContext->makeCurrent();
  if (settings.m_transparentBg)
    glContext->clear(TPixel32::Transparent);
  else
    glContext->clear(settings.m_blackBgCheck ? TPixel::Black : TPixel32::White);
  prepareIconGL(glContext, iconSize);

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);
  glPushMatrix();
  tglMultMatrix(aff);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(0.0f, 1.0f, 0.0f, 0.7f);
  tglDrawEdges(*mi);
  glPopMatrix();
  glPopAttrib();

  TRaster32P ras(iconSize);
  glContext->getRaster(ras);
  glContext->doneCurrent();

  return ras;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TImageP image, int frame, const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  TRasterImageP ri(image);
  if (ri) return convertToIcon(ri, iconSize);

  TToonzImageP ti(image);
  if (ti) return convertToIcon(ti, frame, iconSize, settings);

  TVectorImageP vi(image);
  if (vi) return convertToIcon(vi, frame, iconSize, settings);

  TMeshImageP mi(image);
  if (mi) return convertToIcon(mi, frame, iconSize, settings);

  return TRaster32P();
}

}  // namespace

//=============================================================================

//============================
//
//    IconRenderer class
//
//----------------------------

class IconRenderer : public TThread::Runnable {
  TRaster32P m_icon;
  TDimension m_iconSize;
  std::string m_id;

  bool m_started;
  bool m_terminated;

public:
  IconRenderer(const std::string &id, const TDimension &iconSize);
  virtual ~IconRenderer();

  void run() override = 0;

  void setIcon(const TRaster32P &icon) { m_icon = icon; }
  TRaster32P getIcon() const { return m_icon; }

  TDimension getIconSize() { return m_iconSize; }
  const std::string &getId() const { return m_id; }

  bool &hasStarted() { return m_started; }
  bool &wasTerminated() { return m_terminated; }
};

//-----------------------------------------------------------------------------

IconRenderer::IconRenderer(const std::string &id, const TDimension &iconSize)
    : m_icon()
    , m_iconSize(iconSize)
    , m_id(id)
    , m_started(false)
    , m_terminated(false) {
  connect(this, &IconRenderer::started, IconGenerator::instance(),
          &IconGenerator::onStarted);
  connect(this, &IconRenderer::finished, IconGenerator::instance(),
          &IconGenerator::onFinished);
  connect(this, &IconRenderer::canceled, IconGenerator::instance(),
          &IconGenerator::onCanceled, Qt::QueuedConnection);
  connect(this, &IconRenderer::terminated, IconGenerator::instance(),
          &IconGenerator::onTerminated, Qt::QueuedConnection);
}

//-----------------------------------------------------------------------------

IconRenderer::~IconRenderer() {}

//=============================================================================

//===================================
//
//    Specific icon renderers
//
//-----------------------------------

//=============================================================================

//======================================
//    NoImageIconRenderer class
//--------------------------------------

class NoImageIconRenderer final : public IconRenderer {
public:
  NoImageIconRenderer(const std::string &id, const TDimension &iconSize)
      : IconRenderer(id, iconSize) {}
  void run() override {
    try {
      TRaster32P ras(getIconSize());
      ras->fill(TPixel32::Gray);
      setIcon(ras);
    } catch (...) {
    }
  }
};

//=============================================================================

//======================================
//    VectorImageIconRenderer class
//--------------------------------------

class VectorImageIconRenderer final : public IconRenderer {
  TVectorImageP m_vimage;
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

public:
  VectorImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TXshSimpleLevelP sl, const TFrameId &fid,
                          const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_vimage()
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings) {}

  VectorImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TVectorImageP vimage,
                          const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_vimage(vimage)
      , m_sl(0)
      , m_fid(-1)
      , m_settings(settings) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P VectorImageIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  TVectorImageP vimage;

  int frame = 0;
  if (!m_vimage) {
    assert(m_sl);
    if (!m_sl->isFid(m_fid)) return TRaster32P();
    TImageP image = m_sl->getFrameIcon(m_fid);
    if (!image) return TRaster32P();
    vimage = (TVectorImageP)image;
    if (!vimage) return TRaster32P();
    frame = m_sl->guessIndex(m_fid);
  } else
    vimage = m_vimage;
  assert(vimage);

  TRaster32P ras(convertToIcon(vimage, frame, iconSize, m_settings));

  return ras;
}

//-----------------------------------------------------------------------------

void VectorImageIconRenderer::run() {
  try {
    TRaster32P ras(generateRaster(getIconSize()));

    if (ras) setIcon(ras);
  } catch (...) {
  }
}

//=============================================================================

//======================================
//    SplineImageIconRenderer class
//--------------------------------------

class SplineIconRenderer final : public IconRenderer {
  TStageObjectSpline *m_spline;

public:
  SplineIconRenderer(const std::string &id, const TDimension &iconSize,
                     TStageObjectSpline *spline)
      : IconRenderer(id, iconSize), m_spline(spline) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P SplineIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  // get the glContext
  TOfflineGL *glContext =
      IconGenerator::instance()->getOfflineGLContext(iconSize);
  glContext->makeCurrent();
  glContext->clear(TPixel32::White);
  prepareIconGL(glContext, iconSize);

  const TStroke *stroke = m_spline->getStroke();
  assert(stroke);
  if (!stroke) {
    glContext->doneCurrent();
    return TRaster32P();
  }
  TRectD sbbox = stroke->getBBox();

  glColor3d(0, 0, 0);
  double scaleX = 1, scaleY = 1;
  if (sbbox.getLx() > 0.0) scaleX = (double)iconSize.lx / sbbox.getLx();
  if (sbbox.getLy() > 0.0) scaleY = (double)iconSize.ly / sbbox.getLy();
  double scale         = 0.8 * std::min(scaleX, scaleY);
  TPointD centerStroke = 0.5 * (sbbox.getP00() + sbbox.getP11());
  TPointD centerPixmap(iconSize.lx * 0.5, iconSize.ly * 0.5);
  glPushMatrix();
  tglMultMatrix(TScale(scale).place(centerStroke, centerPixmap));
  int n = 50;
  glBegin(GL_LINE_STRIP);
  for (int i = 0; i < n; i++)
    tglVertex(stroke->getPoint((double)i / (double)(n - 1)));
  glEnd();
  glPopMatrix();

  TRaster32P ras(iconSize);
  glContext->getRaster(ras);
  glContext->doneCurrent();
  return ras;
}

//-----------------------------------------------------------------------------

void SplineIconRenderer::run() {
  TRaster32P raster = generateRaster(getIconSize());
  if (raster) setIcon(raster);
}

//=============================================================================

//======================================
//    RasterImageIconRenderer class
//--------------------------------------

class RasterImageIconRenderer final : public IconRenderer {
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;

public:
  RasterImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TXshSimpleLevelP sl, const TFrameId &fid)
      : IconRenderer(id, iconSize), m_sl(sl), m_fid(fid) {}

  void run() override;
};

//-----------------------------------------------------------------------------

void RasterImageIconRenderer::run() {
  if (!m_sl->isFid(m_fid)) return;

  TImageP image = m_sl->getFrameIcon(m_fid);
  if (!image) return;

  TRasterImageP rimage = (TRasterImageP)image;
  assert(rimage);

  TRaster32P icon(convertToIcon(rimage, getIconSize()));

  if (icon) setIcon(icon);
}

//=============================================================================

//======================================
//    ToonzImageIconRenderer class
//--------------------------------------

class ToonzImageIconRenderer final : public IconRenderer {
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

  TRasterCM32P m_tnzImgIcon;

public:
  ToonzImageIconRenderer(const std::string &id, const TDimension &iconSize,
                         TXshSimpleLevelP sl, const TFrameId &fid,
                         const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings)
      , m_tnzImgIcon(0) {}

  void run() override;

  void setIcon_TnzImg(const TRasterCM32P &timgp) { m_tnzImgIcon = timgp; }
  TRasterCM32P getIcon_TnzImg() const { return m_tnzImgIcon; }
};

//-----------------------------------------------------------------------------

void ToonzImageIconRenderer::run() {
  if (!m_sl->isFid(m_fid)) return;

  TImageP image = m_sl->getFrameIcon(m_fid);
  if (!image) return;

  TRasterImageP rimage(image);
  if (rimage) {
    TRaster32P icon(convertToIcon(rimage, getIconSize()));
    if (icon) setIcon(icon);

    return;
  }

  TToonzImageP timage = (TToonzImageP)image;

  TDimension iconSize(getIconSize());
  if (!timage) {
    TRaster32P p(iconSize.lx, iconSize.ly);
    p->fill(TPixelRGBM32::Yellow);
    setIcon(p);

    return;
  }

  TRasterCM32P rasCM32 = timage->getRaster();
  if (!rasCM32.getPointer()) return;

  int lx     = rasCM32->getSize().lx;
  int ly     = rasCM32->getSize().ly;
  int iconLx = iconSize.lx, iconLy = iconSize.ly;

  TRaster32P icon(iconSize);

  icon->fill(m_settings.m_blackBgCheck ? TPixel::Black : TPixel::White);

  if (lx != iconLx && ly != iconLy) {
    // The icons stored in the tlv file don't have the required size.
    // Fetch the original and iconize it.

    image = m_sl->getFrame(m_fid, ImageManager::dontPutInCache,
                           0);  // 0 uses the level properties' subsampling
    if (!image) return;

    timage = (TToonzImageP)image;
    if (!timage) {
      TRaster32P p(iconSize.lx, iconSize.ly);
      p->fill(TPixelRGBM32::Yellow);
      setIcon(p);

      return;
    }

    rasCM32 = timage->getRaster();
    if (!rasCM32.getPointer()) return;

    TRasterCM32P auxCM32(icon->getSize());
    auxCM32->clear();

    TRop::makeIcon(auxCM32, rasCM32);
    rasCM32 = auxCM32;
  }

  if (!m_sl->getPalette()) return;

  TPaletteP plt = m_sl->getPalette()->clone();
  if (!plt) return;

  int frame = m_sl->guessIndex(m_fid);
  plt->setFrame(frame);

  setIcon_TnzImg(rasCM32);
}

//=============================================================================

//======================================
//    MeshImageIconRenderer class
//--------------------------------------

class MeshImageIconRenderer final : public IconRenderer {
  TMeshImageP m_image;
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

public:
  MeshImageIconRenderer(const std::string &id, const TDimension &iconSize,
                        TXshSimpleLevelP sl, const TFrameId &fid,
                        const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_image()
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings) {}

  MeshImageIconRenderer(const std::string &id, const TDimension &iconSize,
                        TMeshImageP image,
                        const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_image(image)
      , m_sl(0)
      , m_fid(-1)
      , m_settings(settings) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P MeshImageIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  TMeshImageP mi;

  int frame = 0;
  if (!m_image) {
    assert(m_sl);
    if (!m_sl->isFid(m_fid)) return TRaster32P();

    TImageP image = m_sl->getFrameIcon(m_fid);
    if (!image) return TRaster32P();

    mi = (TMeshImageP)image;
    if (!mi) return TRaster32P();

    frame = m_sl->guessIndex(m_fid);
  } else
    mi = m_image;

  assert(mi);

  return convertToIcon(mi, frame, iconSize, m_settings);
}

//-----------------------------------------------------------------------------

void MeshImageIconRenderer::run() {
  try {
    TRaster32P ras(generateRaster(getIconSize()));

    if (ras) setIcon(ras);
  } catch (...) {
  }
}

//=============================================================================

//==================================
//    XsheetIconRenderer class
//----------------------------------

class XsheetIconRenderer final : public IconRenderer {
  TXsheet *m_xsheet;
  int m_row;

public:
  XsheetIconRenderer(const std::string &id, const TDimension &iconSize,
                     TXsheet *xsheet, int row = 0)
      : IconRenderer(id, iconSize), m_xsheet(xsheet), m_row(row) {
    if (m_xsheet) {
      assert(m_xsheet->getRefCount() > 0);
      m_xsheet->addRef();
    }
  }

  ~XsheetIconRenderer() {
    if (m_xsheet) m_xsheet->release();
  }

  static std::string getId(TXshChildLevel *level, int row) {
    return "sub:" + ::to_string(level->getName()) + std::to_string(row);
  }

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P XsheetIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  ToonzScene *scene = m_xsheet->getScene();
  TDimension res    = iconSize;
  if (QCoreApplication::applicationName() == "ToonzPreview") {
    res = scene->getCurrentCamera()->getRes();
    if (res.lx > iconSize.lx || res.ly > iconSize.ly) {
      double sx = (double)iconSize.lx / res.lx;
      double sy = (double)iconSize.ly / res.ly;

      double s = std::min(sx, sy);
      res.lx   = tround(res.lx * s);
      res.ly   = tround(res.ly * s);
    }
  }
  TRaster32P ras(res);

  TPixel32 bgColor = scene->getProperties()->getBgColor();
  bgColor.m        = 255;
  ras->fill(bgColor);

  TImageCache::instance()->setEnabled(false);
  // temporarily disable "Visualize Vector As Raster" option to prevent crash.
  // (see the issue #2862)
  bool rasterizePli               = TXshSimpleLevel::m_rasterizePli;
  TXshSimpleLevel::m_rasterizePli = false;

  // All checks are disabled
  scene->renderFrame(ras, m_row, m_xsheet, false);

  TXshSimpleLevel::m_rasterizePli = rasterizePli;
  TImageCache::instance()->setEnabled(true);

  return ras;
}

//-----------------------------------------------------------------------------

void XsheetIconRenderer::run() {
  TRaster32P ras = generateRaster(getIconSize());
  if (ras) setIcon(ras);
}

//=============================================================================

//================================
//    FileIconRenderer class
//--------------------------------

class FileIconRenderer final : public IconRenderer {
  TFilePath m_path;
  TFrameId m_fid;
  int m_browserBgMode;

public:
  FileIconRenderer(const TDimension &iconSize, const TFilePath &path,
                   const TFrameId &fid, int browserBgMode = 0)
      : IconRenderer(getId(path, fid, iconSize, browserBgMode), iconSize)
      , m_path(path)
      , m_fid(fid)
      , m_browserBgMode(browserBgMode) {}

  static std::string getId(const TFilePath &path, const TFrameId &fid,
                           const TDimension &iconSize = TDimension(80, 60),
                           int browserBgMode = 0);

  void run() override;
};

//-----------------------------------------------------------------------------

std::string FileIconRenderer::getId(const TFilePath &path, const TFrameId &fid,
                                    const TDimension &iconSize,
                                    int browserBgMode) {
  std::string type(path.getType());
  std::string id;

  auto withSizeSuffix = [&iconSize, browserBgMode](std::string base) {
    // Type icons are sized; do not share the default-size cache key.
    if (iconSize.lx != 80 || iconSize.ly != 60)
      base += "_r_" + std::to_string(iconSize.lx) + "x" +
              std::to_string(iconSize.ly);
    if (browserBgMode != 0)
      base += "_bg" + std::to_string(browserBgMode);
    return base;
  };

  if (type == "tab" || type == "tnz" ||
      type == "mesh" ||  // meshes are not currently viewable
      TFileType::isViewable(TFileType::getInfo(path)) || type == "tlv" ||
      type == "pli") {
    std::string fidNumber;
    if (fid != TFrameId::NO_FRAME)
      fidNumber = "frame:" + fid.expand(TFrameId::NO_PAD);
    return withSizeSuffix("$:" + ::to_string(path) + fidNumber);
  }

  // All the other types whose icon is the same for file type, get the same id
  // per type (plus size suffix when not default).
  else if (type == "tpl")
    return withSizeSuffix("$:tpl");
  else if (type == "tzp")
    return withSizeSuffix("$:tzp");
  else if (type == "svg")
    return withSizeSuffix("$:svg");
  else if (type == "tzu")
    return withSizeSuffix("$:tzu");
  else if (TFileType::getInfo(path) == TFileType::AUDIO_LEVEL)
    return withSizeSuffix("$:audio");
  else if (type == "scr")
    return withSizeSuffix("$:scr");
  else if (type == "mpath")
    return withSizeSuffix("$:mpath");
  else if (type == "curve")
    return withSizeSuffix("$:curve");
  else if (type == "cln")
    return withSizeSuffix("$:cln");
  else if (type == "tnzbat")
    return withSizeSuffix("$:tnzbat");
  else if (type == "tls")
    return withSizeSuffix("$:tls");
  else if (type == "xdts")
    return withSizeSuffix("$:xdts");
  else if (type == "js")
    return withSizeSuffix("$:js");
  else if (type == "json")
    return withSizeSuffix("$:json");
  else if (type == "psd")
    return withSizeSuffix("$:psd");
  else
    return withSizeSuffix("$:unknown");
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateVectorFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize,
                                                 const TFrameId &fid,
                                                 const Settings &settings) {
  TLevelReaderP lr(path);
  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return TRaster32P();
  TFrameId frameId = fid;
  if (fid == TFrameId::NO_FRAME) frameId = level->begin()->first;
  TImageP img      = lr->getFrameReader(frameId)->load();
  TVectorImageP vi = img;
  if (!vi) return TRaster32P();
  vi->setPalette(level->getPalette());
  VectorImageIconRenderer vir("", iconSize, vi.getPointer(), settings);
  return vir.generateRaster(iconSize);
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateRasterFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize,
                                                 const TFrameId &fid,
                                                 const Settings &settings) {
  TImageP img;

  try {
    // Attempt image reading
    TLevelReaderP lr(path);
    TLevelP level = lr->loadInfo();

    if (level->begin() == level->end()) return TRaster32P();

    TFrameId frameId = fid;
    if (fid == TFrameId::NO_FRAME)  // In case no frame was specified, pick the
      frameId = level->begin()->first;  // first level frame

    TImageReaderP ir = lr->getFrameReader(frameId);

    if (const TImageInfo *ii = ir->getImageInfo()) {
      int shrinkX = ii->m_lx / iconSize.lx;
      int shrinkY = ii->m_ly / iconSize.ly;
      int shrink  = shrinkX < shrinkY ? shrinkX : shrinkY;

      if (shrink > 1) ir->setShrink(shrink);
    }
    bool isDll = QCoreApplication::applicationName() == "ToonzPreview";
    // loadIcon() is only sharp enough for small thumbs; use the full frame
    // when a larger size is requested.
    const bool tlvSmallIcon = toUpper(path.getType()) == "TLV" && !isDll &&
                              iconSize.lx <= 80 && iconSize.ly <= 60;
    img = tlvSmallIcon ? ir->loadIcon() : ir->load();
  } catch (...) {
  }

  // Extract a 32-bit fullcolor raster from img
  TRaster32P ras32;

  if (TRasterImageP ri = img) {
    ras32 = ri->getRaster();

    if (!ras32) {
      if (TRasterGR8P rasGR8 = ri->getRaster()) {
        TRaster32P raux(rasGR8->getSize());
        TRop::convert(raux, rasGR8);
        ras32 = raux;
      }
    }
  } else if (TToonzImageP ti = img) {
    TRasterCM32P auxRaster = ti->getRaster();
    TRaster32P dstRaster(auxRaster->getSize());

    if (TPaletteP plt = ti->getPalette())
      TRop::convert(dstRaster, auxRaster, plt, false);
    else
      dstRaster->fill(TPixel32::Magenta);
    ras32 = TRaster32P(dstRaster->getLx(), dstRaster->getLy());
    if (settings.m_transparentBg)
      ras32->clear();
    else
      ras32->fill(TPixel32::White);
    TRop::over(ras32, dstRaster);
  }

  if (!ras32) return TRaster32P();

  /*
// NOTE: The following was possible with the old Qt version 4.3.3 - but in the
new 4.5.0
// it's not: 'It is not safe to use QPixmaps outside the GUI thread'...
TRaster32P icon;
{
QPixmap p(rasterToQPixmap(ras32));
icon = rasterFromQPixmap(
  scalePixmapKeepingAspectRatio(p, QSize(iconSize.lx, iconSize.ly),
Qt::transparent)
  , false);
}
*/

  double sx = double(iconSize.lx) / ras32->getLx();
  double sy = double(iconSize.ly) / ras32->getLy();
  double sc = std::min(sx, sy);

  TDimension finalIconSize(tround(ras32->getLx() * sc),
                           tround(ras32->getLy() * sc));

  TRaster32P icon(finalIconSize);

  TAffine aff = TScale(sc).place(ras32->getCenterD(), icon->getCenterD());

  // Fill letterbox before resample.
  if (settings.m_transparentBg)
    icon->clear();
  else
    icon->fill(TPixel32::White);

  // ClosestPixel for small thumbs; Triangle when downscaling to larger ones.
  const TRop::ResampleFilterType filter =
      (iconSize.lx > 80 || iconSize.ly > 60) ? TRop::Triangle
                                             : TRop::ClosestPixel;
  TRop::resample(icon, ras32, aff, filter);

  if (icon) {
    if (::isUnpremultiplied(icon))  // APPALLING. I'm not touching this, but
      TRop::premultiply(
          icon);  // YOU JUST CAN'T TELL IF AN IMAGE IS PREMULTIPLIED
                  // OR NOT BY SCANNING ITS PIXELS.
                  // You either know it FOR A GIVEN, or you don't...      >_<
  } else
    icon->fill(TPixel32(255, 0, 0));

  return icon;
}
//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateSplineFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize) {
  TStageObjectSpline *spline = new TStageObjectSpline();
  TIStream is(path);
  spline->loadData(is);
  SplineIconRenderer sr("", iconSize, spline);
  TRaster32P icon = sr.generateRaster(iconSize);
  delete spline;
  return icon;
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateMeshFileIcon(const TFilePath &path,
                                               const TDimension &iconSize,
                                               const TFrameId &fid,
                                               const Settings &settings) {
  TLevelReaderP lr(path);
  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return TRaster32P();

  TFrameId frameId = fid;
  if (fid == TFrameId::NO_FRAME) frameId = level->begin()->first;

  TMeshImageP mi = lr->getFrameReader(frameId)->load();
  if (!mi) return TRaster32P();

  MeshImageIconRenderer mir("", iconSize, mi.getPointer(), settings);
  return mir.generateRaster(iconSize);
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateSceneFileIcon(const TFilePath &path,
                                                const TDimension &iconSize,
                                                int row) {
  if (row == 0 || row == TFrameId::NO_FRAME - 1) {
    TFilePath iconPath =
        path.getParentDir() + "sceneIcons" + (path.getWideName() + L" .png");
    return generateRasterFileIcon(iconPath, iconSize, TFrameId::NO_FRAME);
  } else {
    if (row < 0) row = 0;
    // obsolete
    ToonzScene scene;
    try {
      scene.load(path);
    } catch (...) {
      scene.clear();
      return TRaster32P();
    }
    XsheetIconRenderer ir("", iconSize, scene.getXsheet(), row);
    return ir.generateRaster(iconSize);
  }
}

//-----------------------------------------------------------------------------

void FileIconRenderer::run() {
  TDimension iconSize(getIconSize());
  const QSize qSize(iconSize.lx, iconSize.ly);

  // Render decorations at the requested size instead of upscaling a tiny SVG.
  auto setSvgDecoration = [this, &qSize](const QString &svgPath) {
    QImage img =
        svgToImage(svgPath, qSize, Qt::KeepAspectRatio, Qt::transparent);
    if (!img.isNull()) setIcon(rasterFromQImage(img));
  };
  auto setRasterDecoration = [this, &qSize](const QString &imgPath) {
    QImage img(imgPath);
    if (img.isNull()) return;
    if (img.size() != qSize)
      img = img.scaled(qSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    setIcon(rasterFromQImage(img));
  };

  try {
    TRaster32P iconRaster;
    std::string type(m_path.getType());
    const IconGenerator::Settings settings =
        browserFileIconSettings(m_browserBgMode);

    if (type == "tnz" || type == "tab")
      iconRaster = IconGenerator::generateSceneFileIcon(m_path, iconSize,
                                                        m_fid.getNumber() - 1);
    else if (type == "pli")
      iconRaster = IconGenerator::generateVectorFileIcon(m_path, iconSize, m_fid,
                                                       settings);
    else if (type == "tpl") {
      setSvgDecoration(QStringLiteral(":Resources/paletteicon.svg"));
      return;
    } else if (type == "tzp") {
      setRasterDecoration(QStringLiteral(":Resources/tzpicon.png"));
      return;
    } else if (type == "svg") {
      setSvgDecoration(getIconPath("svg_icon"));
      return;
    } else if (type == "tzu") {
      setRasterDecoration(QStringLiteral(":Resources/tzuicon.png"));
      return;
    } else if (TFileType::getInfo(m_path) == TFileType::AUDIO_LEVEL) {
      setSvgDecoration(getIconPath("audio_icon"));
      return;
    } else if (type == "scr") {
      setRasterDecoration(QStringLiteral(":Resources/savescreen.png"));
      return;
    } else if (type == "psd") {
      setSvgDecoration(getIconPath("psd_icon"));
      return;
    } else if (type == "mesh")
      iconRaster =
          IconGenerator::generateMeshFileIcon(m_path, iconSize, m_fid, settings);
    else if (TFileType::isViewable(TFileType::getInfo(m_path)) || type == "tlv")
      iconRaster = IconGenerator::generateRasterFileIcon(m_path, iconSize, m_fid,
                                                       settings);
    else if (type == "mpath") {
      setSvgDecoration(getIconPath("motionpath_icon"));
      return;
    } else if (type == "curve") {
      setSvgDecoration(getIconPath("curve_icon"));
      return;
    } else if (type == "cln") {
      setSvgDecoration(getIconPath("cleanup_icon"));
      return;
    } else if (type == "tnzbat") {
      setSvgDecoration(getIconPath("tasklist_icon"));
      return;
    } else if (type == "tls") {
      setSvgDecoration(QStringLiteral(":Resources/magpie.svg"));
      return;
    } else if (type == "xdts") {
      setSvgDecoration(getIconPath("xdts_icon"));
      return;
    } else if (type == "js") {
      setSvgDecoration(getIconPath("script_icon"));
      return;
    } else if (type == "json") {
      setSvgDecoration(getIconPath("json_icon"));
      return;
    }

    else {
      setSvgDecoration(getIconPath("unknown_icon"));
      return;
    }
    if (!iconRaster) {
      setSvgDecoration(getIconPath("broken_icon"));
      return;
    }
    setIcon(iconRaster);
  } catch (const TImageVersionException &) {
    setSvgDecoration(getIconPath("unknown_icon"));
  } catch (...) {
    setSvgDecoration(getIconPath("broken_icon"));
  }
}

//=============================================================================

//================================
//    SceneIconRenderer class
//--------------------------------

class SceneIconRenderer final : public IconRenderer {
  ToonzScene *m_toonzScene;

public:
  SceneIconRenderer(const TDimension &iconSize, ToonzScene *scene)
      : IconRenderer(getId(), iconSize), m_toonzScene(scene) {}

  static std::string getId() { return "currentScene"; }

  void run() override;
  TRaster32P generateIcon(const TDimension &iconSize) const;
};

//-----------------------------------------------------------------------------

TRaster32P SceneIconRenderer::generateIcon(const TDimension &iconSize) const {
  TRaster32P ras(iconSize);

  TPixel32 bgColor = m_toonzScene->getProperties()->getBgColor();
  bgColor.m        = 255;
  ras->fill(bgColor);

  m_toonzScene->renderFrame(ras, 0, 0, false);

  return ras;
}

//-----------------------------------------------------------------------------

void SceneIconRenderer::run() { setIcon(generateIcon(getIconSize())); }

//=============================================================================

//===================================
//
//    IconGenerator class
//
//-----------------------------------

IconGenerator::IconGenerator() : m_iconSize(FilmstripIconSize) {
  m_executor.setMaxActiveTasks(1);  // Only one thread to render icons...
  m_executor.setDedicatedThreads(true);
}

//-----------------------------------------------------------------------------

IconGenerator::~IconGenerator() {}

//-----------------------------------------------------------------------------

IconGenerator *IconGenerator::instance() {
  bool isShellExtension = false;
  if (QCoreApplication::instance()) {
    if (QCoreApplication::applicationName() == "ToonzPreview") {
      isShellExtension = true;
    }
  }
  if (isShellExtension) {
    static IconGenerator *_instance = new IconGenerator();
    return _instance;
  }
  static IconGenerator _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void IconGenerator::setFilmstripIconSize(const TDimension &dim) {
  FilmstripIconSize = dim;
}

//-----------------------------------------------------------------------------

TDimension IconGenerator::getIconSize() const { return FilmstripIconSize; }

//-----------------------------------------------------------------------------

TOfflineGL *IconGenerator::getOfflineGLContext(const TDimension &minSize) {
  // Size the buffer for the request (and filmstrip / cast prefs). Grow the
  // TLS OfflineGL in place; tearing it down on worker threads is unsafe.
  const TDimension requiredSize(
      std::max(minSize.lx, std::max(FilmstripIconSize.lx, IconSize.lx)),
      std::max(minSize.ly, std::max(FilmstripIconSize.ly, IconSize.ly)));

  TOfflineGL *context = m_contexts.localData();
  if (!context) {
    context = new TOfflineGL(requiredSize);
    m_contexts.setLocalData(context);
    return context;
  }

  const TDimension actualSize = context->getSize();
  if (actualSize.lx < requiredSize.lx || actualSize.ly < requiredSize.ly) {
    // Keep the previous smaller context; replacing TLS only.
    context = new TOfflineGL(requiredSize);
    m_contexts.setLocalData(context);
  }

  return context;
}
//-----------------------------------------------------------------------------

void IconGenerator::addTask(const std::string &id,
                            TThread::RunnableP iconRenderer) {
  iconsMap.insert(id);
  m_executor.addTask(iconRenderer);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(TXshLevel *xl, const TFrameId &fid,
                               bool filmStrip, bool onDemand) {
  if (!xl) return QPixmap();

  if (TXshChildLevel *cl = xl->getChildLevel()) {
    if (filmStrip) return QPixmap();

    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    QPixmap pix;
    if (::getIcon(id, pix)) return pix;

    if (onDemand) return pix;

    TDimension iconSize = TDimension(80, 60);

    // The icon must be calculated - add an IconRenderer task.
    // storeIcon(id, QPixmap());   //It was automatically added by the former
    // access
    addTask(id, new XsheetIconRenderer(id, iconSize, cl->getXsheet()));
  }

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    // make thumbnails for cleanup preview and cameratest to be the same as
    // normal TLV
    std::string id;
    int status = sl->getFrameStatus(fid);
    if (sl->getType() == TZP_XSHLEVEL &&
        status & TXshSimpleLevel::CleanupPreview) {
      sl->setFrameStatus(fid, status & ~TXshSimpleLevel::CleanupPreview);
      id = sl->getIconId(fid);
      sl->setFrameStatus(fid, status);
    } else
      id = sl->getIconId(fid);

    if (!filmStrip) id += "_small";

    QPixmap pix;
    if (::getIcon(id, pix, xl->getSimpleLevel())) return pix;

    if (onDemand) return pix;

    IconGenerator::Settings oldSettings = m_settings;

    // Disable transparency check for cast and xsheet icons
    if (!filmStrip) m_settings = IconGenerator::Settings();

    TDimension iconSize = filmStrip ? m_iconSize : TDimension(80, 60);

    // storeIcon(id, QPixmap());

    int type = sl->getType();
    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id,
              new VectorImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case TZP_XSHLEVEL:
      // Yep, we could have rasters, due to a cleanupping process
      if (status == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      else
        addTask(id,
                new ToonzImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    default:
      addTask(id, new NoImageIconRenderer(id, iconSize));
      break;
    }

    m_settings = oldSettings;
  }

  return QPixmap();
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getSizedIcon(TXshLevel *xl, const TFrameId &fid,
                                    std::string newId, TDimension dim) {
  if (!xl) return QPixmap();

  if (TXshChildLevel *cl = xl->getChildLevel()) {
    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    QPixmap pix;
    if (::getIcon(id, pix)) return pix;

    // if (onDemand) return pix;

    TDimension iconSize = TDimension(80, 60);
    if (dim != TDimension(0, 0)) {
      iconSize = dim;
    }

    // The icon must be calculated - add an IconRenderer task.
    // storeIcon(id, QPixmap());   //It was automatically added by the former
    // access
    addTask(id, new XsheetIconRenderer(id, iconSize, cl->getXsheet()));
  }

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    // make thumbnails for cleanup preview and cameratest to be the same as
    // normal TLV
    std::string id;
    int status = sl->getFrameStatus(fid);
    if (sl->getType() == TZP_XSHLEVEL &&
        status & TXshSimpleLevel::CleanupPreview) {
      sl->setFrameStatus(fid, status & ~TXshSimpleLevel::CleanupPreview);
      id = sl->getIconId(fid);
      sl->setFrameStatus(fid, status);
    } else
      id = sl->getIconId(fid);

    id += newId;

    QPixmap pix;
    if (::getIcon(id, pix, xl->getSimpleLevel())) return pix;

    // if (onDemand) return pix;

    IconGenerator::Settings oldSettings = m_settings;

    // Disable transparency check for cast and xsheet icons
    // if (!filmStrip) m_settings = IconGenerator::Settings();

    TDimension iconSize = TDimension(80, 60);
    if (dim != TDimension(0, 0)) {
      iconSize = dim;
    }

    // storeIcon(id, QPixmap());

    int type = sl->getType();
    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id,
              new VectorImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case TZP_XSHLEVEL:
      // Yep, we could have rasters, due to a cleanupping process
      if (status == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      else
        addTask(id,
                new ToonzImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    default:
      assert(false);
      break;
    }

    m_settings = oldSettings;
  }

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(TXshLevel *xl, const TFrameId &fid,
                               bool onlyFilmStrip) {
  if (!xl) return;

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    std::string id = sl->getIconId(fid);
    removeResponsiveSizedIcons(id);

    int type = sl->getType();

    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, getIconSize(), sl, fid));
      break;
    case PLI_XSHLEVEL:
      removeIcon(id);
      addTask(id, new VectorImageIconRenderer(id, getIconSize(), sl, fid,
                                              m_settings));
      break;
    case TZP_XSHLEVEL:
      if (sl->getFrameStatus(fid) == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, getIconSize(), sl, fid));
      else
        addTask(id, new ToonzImageIconRenderer(id, getIconSize(), sl, fid,
                                               m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, getIconSize(), sl, fid,
                                            m_settings));
      break;
    default:
      addTask(id, new NoImageIconRenderer(id, getIconSize()));
      break;
    }

    if (onlyFilmStrip) return;

    id += "_small";
    if (iconsMap.find(id) == iconsMap.end()) return;

    // Not-filmstrip icons disable all checks
    IconGenerator::Settings oldSettings = m_settings;
    m_settings.m_transparencyCheck      = false;
    m_settings.m_inkIndex               = -1;
    m_settings.m_paintIndex             = -1;
    m_settings.m_blackBgCheck           = false;

    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, TDimension(80, 60), sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id, new VectorImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                              m_settings));
      break;
    case TZP_XSHLEVEL:
      if (sl->getFrameStatus(fid) == TXshSimpleLevel::Scanned)
        addTask(id,
                new RasterImageIconRenderer(id, TDimension(80, 60), sl, fid));
      else
        addTask(id, new ToonzImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                               m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                            m_settings));
      break;
    default:
      addTask(id, new NoImageIconRenderer(id, TDimension(80, 60)));
      break;
    }

    m_settings = oldSettings;
  } else if (TXshChildLevel *cl = xl->getChildLevel()) {
    if (onlyFilmStrip) return;

    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    removeIcon(id);

    getIcon(xl, fid);
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(TXshLevel *xl, const TFrameId &fid,
                           bool onlyFilmStrip) {
  if (!xl) return;
  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    std::string id(sl->getIconId(fid));

    removeIcon(id);
    removeResponsiveSizedIcons(id);
    if (!onlyFilmStrip) removeIcon(id + "_small");
  } else {
    TXshChildLevel *cl = xl->getChildLevel();
    if (cl && !onlyFilmStrip)
      removeIcon(XsheetIconRenderer::getId(cl, fid.getNumber() - 1));
  }
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(TStageObjectSpline *spline) {
  if (!spline) return QPixmap();
  std::string iconName = spline->getIconId();

  QPixmap pix;
  if (::getIcon(iconName, pix)) return pix;

  // storeIcon(id, QPixmap());
  addTask(iconName, new SplineIconRenderer(iconName, getIconSize(), spline));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(TStageObjectSpline *spline) {
  if (!spline) return;
  std::string iconName = spline->getIconId();
  removeIcon(iconName);

  addTask(iconName, new SplineIconRenderer(iconName, getIconSize(), spline));
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(TStageObjectSpline *spline) {
  if (!spline) return;
  std::string iconName = spline->getIconId();
  removeIcon(iconName);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(const TFilePath &path, const TFrameId &fid) {
  return getSizedIcon(path, TDimension(80, 60), fid);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getSizedIcon(const TFilePath &path,
                                    const TDimension &dim,
                                    const TFrameId &fid,
                                    int browserBgMode) {
  TDimension fileIconSize =
      (dim.lx > 0 && dim.ly > 0) ? dim : TDimension(80, 60);
  std::string id =
      FileIconRenderer::getId(path, fid, fileIconSize, browserBgMode);

  QPixmap pix;
  // fileIconSize checks high-dpi (devPixRatio > 1.0) cache entries.
  if (::getIcon(id, pix, 0, fileIconSize)) {
    // Pending (null) or ready — do not enqueue a duplicate task.
    return pix;
  }

  addTask(id, new FileIconRenderer(fileIconSize, path, fid, browserBgMode));

  return QPixmap();
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::peekSizedIcon(const TFilePath &path,
                                     const TDimension &dim,
                                     const TFrameId &fid,
                                     int browserBgMode) {
  TDimension fileIconSize =
      (dim.lx > 0 && dim.ly > 0) ? dim : TDimension(80, 60);
  std::string id =
      FileIconRenderer::getId(path, fid, fileIconSize, browserBgMode);
  QPixmap pix;
  if (::getIcon(id, pix, 0, fileIconSize) && !pix.isNull()) return pix;
  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(const TFilePath &path, const TFrameId &fid) {
  std::string id = FileIconRenderer::getId(path, fid);
  removeIcon(id);
  removeResponsiveSizedIcons(id);
  addTask(id, new FileIconRenderer(TDimension(80, 60), path, fid));
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(const TFilePath &path, const TFrameId &fid) {
  std::string id = FileIconRenderer::getId(path, fid);
  removeIcon(id);
  removeResponsiveSizedIcons(id);
}

//-----------------------------------------------------------------------------

void IconGenerator::purgeResponsiveFileIconsExcept(const TDimension &keepA,
                                                  const TDimension &keepB) {
  auto suffixOf = [](const TDimension &d) -> std::string {
    if (d.lx <= 0 || d.ly <= 0) return std::string();
    return "_r_" + std::to_string(d.lx) + "x" + std::to_string(d.ly);
  };
  const std::string keep1 = suffixOf(keepA);
  const std::string keep2 = suffixOf(keepB);

  std::vector<std::string> toRemove;
  for (const std::string &id : iconsMap) {
    if (id.size() < 4 || id.compare(0, 2, "$:") != 0) continue;
    const size_t rPos  = id.rfind("_r_");
    const bool hasBg   = id.find("_bg") != std::string::npos;
    if (rPos == std::string::npos && !hasBg) continue;

    if (keep1.empty() && keep2.empty()) {
      toRemove.push_back(id);
      continue;
    }

    if (rPos == std::string::npos) continue;
    std::string suf = id.substr(rPos);
    const size_t bgInSuf = suf.find("_bg");
    if (bgInSuf != std::string::npos) suf = suf.substr(0, bgInSuf);
    if (!keep1.empty() && suf == keep1) continue;
    if (!keep2.empty() && suf == keep2) continue;
    toRemove.push_back(id);
  }
  for (const std::string &key : toRemove) removeIcon(key);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getSceneIcon(ToonzScene *scene) {
  std::string id(SceneIconRenderer::getId());

  QPixmap pix;
  if (::getIcon(id, pix)) return pix;

  // storeIcon(id, QPixmap());
  addTask(id, new SceneIconRenderer(getIconSize(), scene));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidateSceneIcon() {
  removeIcon(SceneIconRenderer::getId());
}

//-----------------------------------------------------------------------------

void IconGenerator::remap(const std::string &newIconId,
                          const std::string &oldIconId) {
  IconIterator it = iconsMap.find(oldIconId);
  if (it == iconsMap.end()) return;

  iconsMap.erase(it);
  iconsMap.insert(newIconId);

  TImageCache::instance()->remap(newIconId, oldIconId);
}

//-----------------------------------------------------------------------------

void IconGenerator::clearRequests() { m_executor.cancelAll(); }

//-----------------------------------------------------------------------------

void IconGenerator::clearSceneIcons() {
  // Eliminate all icons whose prefix is not "$:" (that is, scene-independent
  // images).
  // The abovementioned prefix is internally recognized by the image cache when
  // the scene
  // changes to avoid clearing file browser's icons.

  // Observe that image cache's clear function invoked during scene changes is
  // called through
  // the ImageManager::clear() method, including FilmStrip icons.

  // note the ';' - which follows ':' in the ascii table
  iconsMap.erase(iconsMap.begin(), iconsMap.lower_bound("$:"));
  iconsMap.erase(iconsMap.lower_bound("$;"), iconsMap.end());
}

//-----------------------------------------------------------------------------

void IconGenerator::onStarted(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  ir->hasStarted() = true;
}

//-----------------------------------------------------------------------------

void IconGenerator::onCanceled(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  // Drop the map entry so a canceled request is not treated as a hit.
  removeIcon(ir->getId());
}

//-----------------------------------------------------------------------------

void IconGenerator::onFinished(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  // Skip finished work whose map entry was already cleared.
  if (iconsMap.find(ir->getId()) == iconsMap.end()) {
    if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
    return;
  }

  // if the icon was generated in TToonzImage format, cache it instead
  ToonzImageIconRenderer *tir = dynamic_cast<ToonzImageIconRenderer *>(ir);
  if (tir) {
    TRasterCM32P timgp = tir->getIcon_TnzImg();
    if (timgp) {
      ::setIcon_TnzImg(ir->getId(), timgp);
      emit iconGenerated();
      if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
      return;
    }
  }

  // Update the icons map
  if (ir->getIcon()) {
    ::setIcon(ir->getId(), ir->getIcon());
    emit iconGenerated();
  }

  if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
}

//-----------------------------------------------------------------------------

void IconGenerator::onException(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  removeIcon(ir->getId());
  if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
}

//-----------------------------------------------------------------------------

void IconGenerator::onTerminated(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  ir->wasTerminated() = true;
  m_iconsTerminationLoop.exec();
}
