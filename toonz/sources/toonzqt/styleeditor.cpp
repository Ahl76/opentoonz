

#include "toonzqt/styleeditor.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/filefield.h"
#include "historytypes.h"
#include "toonzqt/lutcalibrator.h"

// TnzLib includes
#include "toonz/txshlevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/toonzfolders.h"
#include "toonz/cleanupcolorstyles.h"
#include "toonz/palettecontroller.h"
#include "toonz/imagestyles.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tconvert.h"
#include "tfiletype.h"
#include "tsystem.h"
#include "tundo.h"
#include "tcolorstyles.h"
#include "tpalette.h"
#include "tpixel.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tropcm.h"
#include "tvectorrenderdata.h"
#include "tsimplecolorstyles.h"
#include "tvectorbrushstyle.h"

// Qt includes
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPen>
#include <QButtonGroup>
#include <QMouseEvent>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QSize>
#include <QRadioButton>
#include <QComboBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <algorithm>
#include <cmath>
#include <QStyleOptionSlider>
#include <QToolTip>
#include <QSplitter>
#include <QMenu>
#include <QOpenGLFramebufferObject>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <functional>

namespace {
enum ColorSliderAppearance {
  RelativeColoredTriangleHandle,
  AbsoluteColoredLineHandle
};
}
TEnv::IntVar StyleEditorColorSliderAppearance(
    "StyleEditorColorSliderAppearance", RelativeColoredTriangleHandle);
TEnv::IntVar StyleEditorColorPageMode("StyleEditorColorPageMode",
                                      static_cast<int>(ColorPageMode::Classic));
TEnv::IntVar StyleEditorAdvancedSvShape(
    "StyleEditorAdvancedSvShape", static_cast<int>(AdvancedSvShape::Square));
TEnv::IntVar StyleEditorAdvancedPickerKind(
    "StyleEditorAdvancedPickerKind", static_cast<int>(AdvancedPickerKind::Wheel));
TEnv::IntVar StyleEditorShowAdvancedModeButton(
    "StyleEditorShowAdvancedModeButton", 1);
TEnv::IntVar StyleEditorShowSvShapeButton("StyleEditorShowSvShapeButton", 1);
TEnv::IntVar StyleEditorShowSectionToggles("StyleEditorShowSectionToggles", 1);
TEnv::IntVar StyleEditorShowPickerKindButtons(
    "StyleEditorShowPickerKindButtons", 1);
TEnv::IntVar StyleEditorShowVarButton("StyleEditorShowVarButton", 1);

using namespace StyleEditorGUI;

namespace {

ColorPageMode normalizedColorPageMode(int modeId) {
  if (modeId == static_cast<int>(ColorPageMode::Advanced))
    return ColorPageMode::Advanced;
  return ColorPageMode::Classic;
}

AdvancedSvShape normalizedSvShape(int shapeId) {
  if (shapeId == static_cast<int>(AdvancedSvShape::Triangle))
    return AdvancedSvShape::Triangle;
  return AdvancedSvShape::Square;
}

AdvancedPickerKind normalizedPickerKind(int kindId) {
  if (kindId == static_cast<int>(AdvancedPickerKind::Rectangle))
    return AdvancedPickerKind::Rectangle;
  return AdvancedPickerKind::Wheel;
}

}  // namespace

//*****************************************************************************
//    UndoPaletteChange  definition
//*****************************************************************************

namespace {

class UndoPaletteChange final : public TUndo {
  TPaletteHandle *m_paletteHandle;
  TPaletteP m_palette;

  int m_styleId;
  const TColorStyleP m_oldColor, m_newColor;

  std::wstring m_oldName, m_newName;

  bool m_oldEditedFlag, m_newEditedFlag;

  int m_frame;

public:
  UndoPaletteChange(TPaletteHandle *paletteHandle, int styleId,
                    const TColorStyle &oldColor, const TColorStyle &newColor)
      : m_paletteHandle(paletteHandle)
      , m_palette(paletteHandle->getPalette())
      , m_styleId(styleId)
      , m_oldColor(oldColor.clone())
      , m_newColor(newColor.clone())
      , m_oldName(oldColor.getName())
      , m_newName(newColor.getName())
      , m_frame(m_palette->getFrame())
      , m_oldEditedFlag(oldColor.getIsEditedFlag())
      , m_newEditedFlag(newColor.getIsEditedFlag()) {}

  void undo() const override {
    m_palette->setStyle(m_styleId, m_oldColor->clone());
    m_palette->getStyle(m_styleId)->setIsEditedFlag(m_oldEditedFlag);
    m_palette->getStyle(m_styleId)->setName(m_oldName);

    if (m_palette->isKeyframe(m_styleId, m_frame))
      m_palette->setKeyframe(m_styleId, m_frame);

    // don't change the dirty flag. because m_palette may not the current
    // palette when undo executed
    m_paletteHandle->notifyColorStyleChanged(false, false);
  }

  void redo() const override {
    m_palette->setStyle(m_styleId, m_newColor->clone());
    m_palette->getStyle(m_styleId)->setIsEditedFlag(m_newEditedFlag);
    m_palette->getStyle(m_styleId)->setName(m_newName);

    if (m_palette->isKeyframe(m_styleId, m_frame))
      m_palette->setKeyframe(m_styleId, m_frame);

    // don't change the dirty flag. because m_palette may not the current
    // palette when undo executed
    m_paletteHandle->notifyColorStyleChanged(false, false);
  }

  // imprecise - depends on the style
  int getSize() const override {
    return sizeof(*this) + 2 * sizeof(TColorStyle *);
  }

  QString getHistoryString() override {
    return QObject::tr(
               "Change Style   Palette : %1  Style#%2  [R%3 G%4 B%5] -> [R%6 "
               "G%7 B%8]")
        .arg(QString::fromStdWString(m_palette->getPaletteName()))
        .arg(QString::number(m_styleId))
        .arg(m_oldColor->getMainColor().r)
        .arg(m_oldColor->getMainColor().g)
        .arg(m_oldColor->getMainColor().b)
        .arg(m_newColor->getMainColor().r)
        .arg(m_newColor->getMainColor().g)
        .arg(m_newColor->getMainColor().b);
  }

  int getHistoryType() override { return HistoryType::Palette; }
};

}  // namespace

//*****************************************************************************
//    ColorModel  implementation
//*****************************************************************************

const int ChannelMaxValues[]        = {255, 255, 255, 255, 359, 100, 100};
const int ChannelPairMaxValues[][2] = {{255, 255}, {255, 255}, {255, 255},
                                       {255, 255}, {100, 100}, {359, 100},
                                       {359, 100}};

ColorModel::ColorModel() { memset(m_channels, 0, sizeof m_channels); }

//-----------------------------------------------------------------------------

void ColorModel::rgb2hsv() {
  QColor converter(m_channels[0], m_channels[1], m_channels[2]);
  m_channels[4] =
      std::max(converter.hue(), 0);  // hue() returns -1 for achromatic colors
  m_channels[5] = (int)std::round(converter.saturationF() * 100.);
  m_channels[6] = (int)std::round(converter.valueF() * 100.);
}

//-----------------------------------------------------------------------------

void ColorModel::hsv2rgb() {
  QColor converter =
      QColor::fromHsvF((qreal)m_channels[4] / 360., (qreal)m_channels[5] / 100.,
                       (qreal)m_channels[6] / 100.);

  m_channels[0] = converter.red();
  m_channels[1] = converter.green();
  m_channels[2] = converter.blue();
}

//-----------------------------------------------------------------------------

void ColorModel::setTPixel(const TPixel32 &pix) {
  QColor color(pix.r, pix.g, pix.b, pix.m);
  m_channels[0] = color.red();
  m_channels[1] = color.green();
  m_channels[2] = color.blue();
  m_channels[3] = color.alpha();
  m_channels[4] =
      std::max(color.hue(), 0);  // hue() returns -1 for achromatic colors
  m_channels[5] = (int)std::round(color.saturationF() * 100.);
  m_channels[6] = (int)std::round(color.valueF() * 100.);
}

//-----------------------------------------------------------------------------

TPixel32 ColorModel::getTPixel() const {
  return TPixel32(m_channels[0], m_channels[1], m_channels[2], m_channels[3]);
}

//-----------------------------------------------------------------------------

void ColorModel::setValue(ColorChannel channel, int value) {
  assert(0 <= (int)channel && (int)channel < 7);
  assert(0 <= value && value <= ChannelMaxValues[channel]);
  m_channels[(int)channel] = value;
  if (channel >= eHue)
    hsv2rgb();
  else if (channel <= eBlue)
    rgb2hsv();
}

//-----------------------------------------------------------------------------

void ColorModel::setValues(ColorChannel channel, int v, int u) {
  assert(0 <= (int)channel && (int)channel < 7);
  switch (channel) {
  case eRed:
    setValue(eGreen, v);
    setValue(eBlue, u);
    break;
  case eGreen:
    setValue(eRed, v);
    setValue(eBlue, u);
    break;
  case eBlue:
    setValue(eRed, v);
    setValue(eGreen, u);
    break;
  case eHue:
    setValue(eSaturation, v);
    setValue(eValue, u);
    break;
  case eSaturation:
    setValue(eHue, v);
    setValue(eValue, u);
    break;
  case eValue:
    setValue(eHue, v);
    setValue(eSaturation, u);
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------

int ColorModel::getValue(ColorChannel channel) const {
  assert(0 <= (int)channel && (int)channel < 7);
  return m_channels[(int)channel];
}

//-----------------------------------------------------------------------------

void ColorModel::getValues(ColorChannel channel, int &u, int &v) {
  switch (channel) {
  case eRed:
    u = getValue(eGreen);
    v = getValue(eBlue);
    break;
  case eGreen:
    u = getValue(eRed);
    v = getValue(eBlue);
    break;
  case eBlue:
    u = getValue(eRed);
    v = getValue(eGreen);
    break;
  case eHue:
    u = getValue(eSaturation);
    v = getValue(eValue);
    break;
  case eSaturation:
    u = getValue(eHue);
    v = getValue(eValue);
    break;
  case eValue:
    u = getValue(eHue);
    v = getValue(eSaturation);
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

class RedShadeMaker {
  const ColorModel &m_color;

public:
  RedShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(value, m_color.g(), m_color.b()).rgba();
  }
};

//-----------------------------------------------------------------------------

class GreenShadeMaker {
  const ColorModel &m_color;

public:
  GreenShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(m_color.r(), value, m_color.b()).rgba();
  }
};

//-----------------------------------------------------------------------------

class BlueShadeMaker {
  const ColorModel &m_color;

public:
  BlueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(m_color.r(), m_color.g(), value).rgba();
  }
};

//-----------------------------------------------------------------------------

class AlphaShadeMaker {
  const ColorModel &m_color;

public:
  AlphaShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(m_color.r(), m_color.g(), m_color.b(), value).rgba();
  }
};

//-----------------------------------------------------------------------------

class HueShadeMaker {
  const ColorModel &m_color;

public:
  HueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor::fromHsv(359 * value / 255, m_color.s() * 255 / 100,
                           m_color.v() * 255 / 100)
        .rgba();
  }
};

//-----------------------------------------------------------------------------

class SaturationShadeMaker {
  const ColorModel &m_color;

public:
  SaturationShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor::fromHsv(m_color.h(), value, m_color.v() * 255 / 100).rgba();
  }
};

//-----------------------------------------------------------------------------

class ValueShadeMaker {
  const ColorModel &m_color;

public:
  ValueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor::fromHsv(m_color.h(), m_color.s() * 255 / 100, value).rgba();
  }
};

//-----------------------------------------------------------------------------

class RedGreenShadeMaker {
  const ColorModel &m_color;

public:
  RedGreenShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor(u, v, m_color.b()).rgba();
  }
};

//-----------------------------------------------------------------------------

class RedBlueShadeMaker {
  const ColorModel &m_color;

public:
  RedBlueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor(u, m_color.g(), v).rgba();
  }
};

//-----------------------------------------------------------------------------

class GreenBlueShadeMaker {
  const ColorModel &m_color;

public:
  GreenBlueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor(m_color.r(), u, v).rgba();
  }
};

//-----------------------------------------------------------------------------

class SaturationValueShadeMaker {
  const ColorModel &m_color;

public:
  SaturationValueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor::fromHsv(m_color.h(), u, v).rgba();
  }
};

//-----------------------------------------------------------------------------

class HueValueShadeMaker {
  const ColorModel &m_color;

public:
  HueValueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor::fromHsv(359 * u / 255, m_color.s() * 255 / 100, v).rgba();
  }
};

//-----------------------------------------------------------------------------

class HueSaturationShadeMaker {
  const ColorModel &m_color;

public:
  HueSaturationShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor::fromHsv(359 * u / 255, v, m_color.v() * 255 / 100).rgba();
  }
};

//-----------------------------------------------------------------------------

template <class ShadeMaker>
QPixmap makeLinearShading(const ShadeMaker &shadeMaker, int size,
                          bool isVertical) {
  assert(size > 0);
  QPixmap bgPixmap;
  int i, dx, dy, w = 1, h = 1;
  int x = 0, y = 0;
  if (isVertical) {
    dx = 0;
    dy = -1;
    h  = size;
    y  = size - 1;
  } else {
    dx = 1;
    dy = 0;
    w  = size;
  }
  QImage image(w, h, QImage::Format_ARGB32);
  for (i = 0; i < size; i++) {
    int v = 255 * i / (size - 1);
    image.setPixel(x, y, shadeMaker.shade(v));
    x += dx;
    y += dy;
  }
  return QPixmap::fromImage(image);
}

//-----------------------------------------------------------------------------

QPixmap makeLinearShading(const ColorModel &color, ColorChannel channel,
                          int size, bool isVertical) {
  bool relative =
      ColorSlider::s_slider_appearance == RelativeColoredTriangleHandle;
  switch (channel) {
  case eRed:
    if (isVertical || relative)
      return makeLinearShading(RedShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_r.png").scaled(size, 1);
  case eGreen:
    if (isVertical || relative)
      return makeLinearShading(GreenShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_g.png").scaled(size, 1);
  case eBlue:
    if (isVertical || relative)
      return makeLinearShading(BlueShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_b.png").scaled(size, 1);
  case eAlpha:
    if (isVertical || relative)
      return makeLinearShading(AlphaShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_m.png").scaled(size, 1);
  case eHue:
    return makeLinearShading(HueShadeMaker(color), size, isVertical);
  case eSaturation:
    return makeLinearShading(SaturationShadeMaker(color), size, isVertical);
  case eValue:
    return makeLinearShading(ValueShadeMaker(color), size, isVertical);
  default:
    assert(0);
  }
  return QPixmap();
}

//-----------------------------------------------------------------------------

template <class ShadeMaker>
QPixmap makeSquareShading(const ShadeMaker &shadeMaker, int width, int height) {
  if (width < 2 || height < 2) return QPixmap();
  QImage image(width, height, QImage::Format_RGB32);
  int i, j;
  for (j = 0; j < height; j++) {
    int u = 255 - (255 * j / (height - 1));
    for (i = 0; i < width; i++) {
      int v = 255 * i / (width - 1);
      image.setPixel(i, j, shadeMaker.shade(v, u));
    }
  }
  return QPixmap::fromImage(image);
}

//-----------------------------------------------------------------------------

QPixmap makeSquareShading(const ColorModel &color, ColorChannel channel,
                          int width, int height) {
  switch (channel) {
  case eRed:
    return makeSquareShading(GreenBlueShadeMaker(color), width, height);
  case eGreen:
    return makeSquareShading(RedBlueShadeMaker(color), width, height);
  case eBlue:
    return makeSquareShading(RedGreenShadeMaker(color), width, height);
  case eHue:
    return makeSquareShading(SaturationValueShadeMaker(color), width, height);
  case eSaturation:
    return makeSquareShading(HueValueShadeMaker(color), width, height);
  case eValue:
    return makeSquareShading(HueSaturationShadeMaker(color), width, height);
  default:
    assert(0);
  }
  return QPixmap();
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//*****************************************************************************
//    HexagonalColorWheel  implementation
//*****************************************************************************

HexagonalColorWheel::HexagonalColorWheel(QWidget *parent)
    : GLWidgetForHighDpi(parent)
    , m_bgColor(128, 128, 128)  // default value in case this value does not set
                                // in the style sheet
{
  setObjectName("HexagonalColorWheel");
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setFocusPolicy(Qt::NoFocus);
  m_currentWheel = none;
  m_pageMode     = normalizedColorPageMode((int)StyleEditorColorPageMode);
  m_svShape      = normalizedSvShape((int)StyleEditorAdvancedSvShape);
  if (Preferences::instance()->isColorCalibrationEnabled())
    m_lutCalibrator = new LutCalibrator();
}

//-----------------------------------------------------------------------------

HexagonalColorWheel::~HexagonalColorWheel() {
  if (m_fbo) delete m_fbo;
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::updateColorCalibration() {
  if (Preferences::instance()->isColorCalibrationEnabled()) {
    // prevent to initialize LutCalibrator before this instance is initialized
    // or OT may crash due to missing OpenGL context
    if (m_firstInitialized) {
      cueCalibrationUpdate();
      return;
    }

    makeCurrent();
    if (!m_lutCalibrator)
      m_lutCalibrator = new LutCalibrator();
    else
      m_lutCalibrator->cleanup();
    m_lutCalibrator->initialize();
    connect(context(), SIGNAL(aboutToBeDestroyed()), this,
            SLOT(onContextAboutToBeDestroyed()));
    if (m_lutCalibrator->isValid() && !m_fbo)
      m_fbo = new QOpenGLFramebufferObject(width(), height());
    doneCurrent();
  }
  update();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::showEvent(QShowEvent *) {
  if (m_cuedCalibrationUpdate) {
    updateColorCalibration();
    m_cuedCalibrationUpdate = false;
  }
  const int logicalW = QOpenGLWidget::width();
  const int logicalH = QOpenGLWidget::height();
  if (logicalW > 0 && logicalH > 0 && isValid()) {
    makeCurrent();
    resizeGL(logicalW, logicalH);
    doneCurrent();
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::initializeGL() {
  initializeOpenGLFunctions();

  // to be computed once through the software
  if (m_lutCalibrator && !m_lutCalibrator->isInitialized()) {
    m_lutCalibrator->initialize();
    connect(context(), SIGNAL(aboutToBeDestroyed()), this,
            SLOT(onContextAboutToBeDestroyed()));
  }

  QColor const color = getBGColor();
  glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());

  // Without the following lines, the wheel in a floating style editor
  // disappears on switching the room due to context switching.
  if (m_firstInitialized)
    m_firstInitialized = false;
  else {
    // Logical size: resizeGL multiplies by DPR once.
    resizeGL(QOpenGLWidget::width(), QOpenGLWidget::height());
    update();
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::hexCornerColor(int cornerIndex, float v, float &r,
                                         float &g, float &b) {
  switch (cornerIndex) {
  case 1:
    r = 0.0f;
    g = v;
    b = 0.0f;
    break;
  case 2:
    r = 0.0f;
    g = v;
    b = v;
    break;
  case 3:
    r = 0.0f;
    g = 0.0f;
    b = v;
    break;
  case 4:
    r = v;
    g = 0.0f;
    b = v;
    break;
  case 5:
    r = v;
    g = 0.0f;
    b = 0.0f;
    break;
  case 6:
    r = v;
    g = v;
    b = 0.0f;
    break;
  default:
    r = g = b = v;
    break;
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::computeHexVertices() {
  m_hexTriHeight = m_hexEdgeLen * 0.866f;
  m_wp[0].setX(m_hexEdgeLen);
  m_wp[0].setY(m_hexTriHeight);
  m_wp[1].setX(m_hexEdgeLen * 0.5f);
  m_wp[1].setY(0.0f);
  m_wp[2].setX(0.0f);
  m_wp[2].setY(m_hexTriHeight);
  m_wp[3].setX(m_hexEdgeLen * 0.5f);
  m_wp[3].setY(m_hexTriHeight * 2.0f);
  m_wp[4].setX(m_hexEdgeLen * 1.5f);
  m_wp[4].setY(m_hexTriHeight * 2.0f);
  m_wp[5].setX(m_hexEdgeLen * 2.0f);
  m_wp[5].setY(m_hexTriHeight);
  m_wp[6].setX(m_hexEdgeLen * 1.5f);
  m_wp[6].setY(0.0f);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::computeClassicLayout(int w, int h) {
  float d                 = (w - 5.0f) / 2.5f;
  bool isHorizontallyLong = ((d * 1.732f) < h) ? false : true;

  if (isHorizontallyLong) {
    m_triEdgeLen = (float)h / 1.732f;
    m_triHeight  = (float)h / 2.0f;
    m_wheelPosition.setX(((float)w - (m_triEdgeLen * 2.5f + 5.0f)) / 2.0f);
    m_wheelPosition.setY(0.0f);
  } else {
    m_triEdgeLen = d;
    m_triHeight  = m_triEdgeLen * 0.866f;
    m_wheelPosition.setX(0.0f);
    m_wheelPosition.setY(((float)h - (m_triHeight * 2.0f)) / 2.0f);
  }

  m_hexEdgeLen = m_triEdgeLen;
  computeHexVertices();

  m_leftp[0].setX(m_wp[6].x() + 5.0f);
  m_leftp[0].setY(0.0f);
  m_leftp[1].setX(m_leftp[0].x() + m_triEdgeLen);
  m_leftp[1].setY(m_triHeight * 2.0f);
  m_leftp[2].setX(m_leftp[1].x());
  m_leftp[2].setY(0.0f);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::computeAdvancedSVTriangle() {
  float R  = m_innerRadius;
  float cx = m_circleCenter.x();
  float cy = m_circleCenter.y();
  auto onCircle = [&](float deg) {
    float rad = deg / 180.0f * 3.1415f;
    return QPointF(cx + R * cosf(rad), cy - R * sinf(rad));
  };
  m_leftp[0] = onCircle(30.0f);
  m_leftp[2] = onCircle(150.0f);
  m_leftp[1] = onCircle(270.0f);
  m_triEdgeLen = (float)QLineF(m_leftp[0], m_leftp[2]).length();
  m_triHeight  = (float)QLineF(m_leftp[1], m_circleCenter).length();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::computeAdvancedLayout(int w, int h) {
  const float pad = 4.0f;
  float avail     = std::min((float)w, (float)h) - pad * 2.0f;
  if (avail < 2.0f) avail = 2.0f;
  m_outerRadius = avail * 0.5f;
  m_innerRadius = m_outerRadius * 0.85f;

  m_wheelPosition.setX(((float)w - avail) * 0.5f);
  m_wheelPosition.setY(((float)h - avail) * 0.5f);

  m_circleCenter.setX(m_outerRadius);
  m_circleCenter.setY(m_outerRadius);
  m_wp[0] = m_circleCenter;

  computeAdvancedSVTriangle();
  computeAdvancedSvSquare();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::computeAdvancedSvSquare() {
  const float half = m_innerRadius * 0.70710678f;
  m_svSquare       = QRectF(m_circleCenter.x() - half, m_circleCenter.y() - half,
                            half * 2.0f, half * 2.0f);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::updateLayout(int w, int h) {
  switch (m_pageMode) {
  case ColorPageMode::Advanced:
    computeAdvancedLayout(w, h);
    break;
  case ColorPageMode::Classic:
  default:
    computeClassicLayout(w, h);
    break;
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::setPageMode(ColorPageMode mode) {
  if (m_pageMode == mode) return;
  m_pageMode = mode;
  const int logicalW = QOpenGLWidget::width();
  const int logicalH = QOpenGLWidget::height();
  if (logicalW > 0 && logicalH > 0 && isValid()) {
    makeCurrent();
    resizeGL(logicalW, logicalH);
    doneCurrent();
  }
  update();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::setSvShape(AdvancedSvShape shape) {
  if (m_svShape == shape) return;
  m_svShape = shape;
  if (m_pageMode == ColorPageMode::Advanced) {
    computeAdvancedSVTriangle();
    computeAdvancedSvSquare();
  }
  update();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::refreshLayout() {
  const int logicalW = QOpenGLWidget::width();
  const int logicalH = QOpenGLWidget::height();
  if (logicalW > 0 && logicalH > 0 && isValid()) {
    makeCurrent();
    resizeGL(logicalW, logicalH);
    doneCurrent();
  }
  update();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::resizeGL(int w, int h) {
  w *= getDevPixRatio();
  h *= getDevPixRatio();

  updateLayout(w, h);

  // GL settings
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, (GLdouble)w, (GLdouble)h, 0.0, 1.0, -1.0);

  // remake fbo with new size
  if (m_lutCalibrator && m_lutCalibrator->isValid()) {
    if (m_fbo) delete m_fbo;
    m_fbo = new QOpenGLFramebufferObject(w, h);
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawClassicHexWheel(float v) {
  glBegin(GL_TRIANGLE_FAN);
  glColor3f(v, v, v);
  glVertex2f(m_wp[0].x(), m_wp[0].y());

  for (int i = 1; i <= 6; ++i) {
    float r, g, b;
    hexCornerColor(i, v, r, g, b);
    glColor3f(r, g, b);
    glVertex2f(m_wp[i].x(), m_wp[i].y());
  }
  float r, g, b;
  hexCornerColor(1, v, r, g, b);
  glColor3f(r, g, b);
  glVertex2f(m_wp[1].x(), m_wp[1].y());
  glEnd();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawHueRing() {
  const int segs = 120;
  float cx = m_circleCenter.x();
  float cy = m_circleCenter.y();
  for (int i = 0; i < segs; ++i) {
    float a0 = (float)i / (float)segs * 360.0f;
    float a1 = (float)(i + 1) / (float)segs * 360.0f;
    QColor c0 = QColor::fromHsv((int)a0 % 360, 255, 255);
    QColor c1 = QColor::fromHsv((int)a1 % 360, 255, 255);
    float r0 = a0 / 180.0f * 3.1415f;
    float r1 = a1 / 180.0f * 3.1415f;
    glBegin(GL_QUADS);
    glColor3f(c0.redF(), c0.greenF(), c0.blueF());
    glVertex2f(cx + m_outerRadius * cosf(r0), cy - m_outerRadius * sinf(r0));
    glColor3f(c1.redF(), c1.greenF(), c1.blueF());
    glVertex2f(cx + m_outerRadius * cosf(r1), cy - m_outerRadius * sinf(r1));
    glColor3f(c1.redF(), c1.greenF(), c1.blueF());
    glVertex2f(cx + m_innerRadius * cosf(r1), cy - m_innerRadius * sinf(r1));
    glColor3f(c0.redF(), c0.greenF(), c0.blueF());
    glVertex2f(cx + m_innerRadius * cosf(r0), cy - m_innerRadius * sinf(r0));
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawSatValueTriangle() {
  if (m_pageMode == ColorPageMode::Advanced) {
    const int n   = 24;
    const int hue = m_color.getValue(eHue);
    const QPointF hueV   = m_leftp[0];
    const QPointF blackV = m_leftp[1];
    const QPointF whiteV = m_leftp[2];
    auto emitVert        = [&](int iBlack, int iWhite) {
      const float wB = (float)iBlack / (float)n;
      const float wW = (float)iWhite / (float)n;
      const float wH = 1.0f - wB - wW;
      const QPointF p = wH * hueV + wB * blackV + wW * whiteV;
      const float V   = std::min(std::max(wH + wW, 0.0f), 1.0f);
      const float S =
          (V > 1e-6f) ? std::min(std::max(wH / V, 0.0f), 1.0f) : 0.0f;
      const QColor c = QColor::fromHsv(hue, (int)(S * 255.0f + 0.5f),
                                       (int)(V * 255.0f + 0.5f));
      glColor3f(c.redF(), c.greenF(), c.blueF());
      glVertex2f((float)p.x(), (float)p.y());
    };
    glBegin(GL_TRIANGLES);
    for (int b = 0; b < n; ++b) {
      for (int w = 0; w < n - b; ++w) {
        emitVert(b, w);
        emitVert(b, w + 1);
        emitVert(b + 1, w);
        if (w + 1 < n - b) {
          emitVert(b + 1, w);
          emitVert(b, w + 1);
          emitVert(b + 1, w + 1);
        }
      }
    }
    glEnd();
    return;
  }

  QColor hueCol = QColor().fromHsv(m_color.getValue(eHue), 255, 255);
  glBegin(GL_TRIANGLES);
  glColor3f(hueCol.redF(), hueCol.greenF(), hueCol.blueF());
  glVertex2f(m_leftp[0].x(), m_leftp[0].y());
  glColor3f(0.0f, 0.0f, 0.0f);
  glVertex2f(m_leftp[1].x(), m_leftp[1].y());
  glColor3f(1.0f, 1.0f, 1.0f);
  glVertex2f(m_leftp[2].x(), m_leftp[2].y());
  glEnd();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawSatValueSquare() {
  const int n    = 24;
  const float x0 = (float)m_svSquare.left();
  const float y0 = (float)m_svSquare.top();
  const float w  = std::max((float)m_svSquare.width(), 1.0f);
  const float h  = std::max((float)m_svSquare.height(), 1.0f);
  const int hue  = m_color.getValue(eHue);
  for (int j = 0; j < n; ++j) {
    const float v0 = 1.0f - (float)j / (float)n;
    const float v1 = 1.0f - (float)(j + 1) / (float)n;
    const float yA = y0 + (1.0f - v0) * h;
    const float yB = y0 + (1.0f - v1) * h;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= n; ++i) {
      const float s = (float)i / (float)n;
      const float x = x0 + s * w;
      QColor c0     = QColor::fromHsv(hue, (int)(s * 255.0f + 0.5f),
                                      (int)(v0 * 255.0f + 0.5f));
      QColor c1     = QColor::fromHsv(hue, (int)(s * 255.0f + 0.5f),
                                      (int)(v1 * 255.0f + 0.5f));
      glColor3f(c0.redF(), c0.greenF(), c0.blueF());
      glVertex2f(x, yA);
      glColor3f(c1.redF(), c1.greenF(), c1.blueF());
      glVertex2f(x, yB);
    }
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::paintGL() {
  // call ClearColor() here in order to update bg color when the stylesheet is
  // switched
  QColor const color = getBGColor();
  glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());

  glMatrixMode(GL_MODELVIEW);

  if (m_lutCalibrator && m_lutCalibrator->isValid()) m_fbo->bind();

  glClear(GL_COLOR_BUFFER_BIT);

  float v = (float)m_color.getValue(eValue) / 100.0f;

  glPushMatrix();
  glTranslatef(m_wheelPosition.rx(), m_wheelPosition.ry(), 0.0f);

  if (m_pageMode == ColorPageMode::Advanced) {
    drawHueRing();
    if (m_svShape == AdvancedSvShape::Square)
      drawSatValueSquare();
    else
      drawSatValueTriangle();
  } else {
    drawClassicHexWheel(v);
    drawSatValueTriangle();
  }

  drawCurrentColorMark();
  glPopMatrix();

  if (m_lutCalibrator && m_lutCalibrator->isValid())
    m_lutCalibrator->onEndDraw(m_fbo);
}

//-----------------------------------------------------------------------------

bool HexagonalColorWheel::svTriangleBarycentric(const QPointF &p,
                                                const QPointF &hueV,
                                                const QPointF &blackV,
                                                const QPointF &whiteV,
                                                float &wHue, float &wBlack,
                                                float &wWhite) {
  QPointF v0 = whiteV - hueV;
  QPointF v1 = blackV - hueV;
  QPointF v2 = p - hueV;
  float dot00 = QPointF::dotProduct(v0, v0);
  float dot01 = QPointF::dotProduct(v0, v1);
  float dot02 = QPointF::dotProduct(v0, v2);
  float dot11 = QPointF::dotProduct(v1, v1);
  float dot12 = QPointF::dotProduct(v1, v2);
  float denom = dot00 * dot11 - dot01 * dot01;
  // Degenerate only: outside points keep weights so callers can clamp.
  if (fabs(denom) < 1e-6f) return false;
  float invDenom = 1.0f / denom;
  wWhite         = (dot11 * dot02 - dot01 * dot12) * invDenom;
  wBlack         = (dot00 * dot12 - dot01 * dot02) * invDenom;
  wHue           = 1.0f - wWhite - wBlack;
  return true;
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::svFromTrianglePoint(const QPointF &localPos, int &s,
                                              int &v) const {
  float wHue, wBlack, wWhite;
  if (!svTriangleBarycentric(localPos, m_leftp[0], m_leftp[1], m_leftp[2], wHue,
                             wBlack, wWhite)) {
    // Degenerate triangle: keep the current color.
    s = m_color.getValue(eSaturation);
    v = m_color.getValue(eValue);
    return;
  }
  wHue   = std::max(0.0f, wHue);
  wBlack = std::max(0.0f, wBlack);
  wWhite = std::max(0.0f, wWhite);
  float sum = wHue + wBlack + wWhite;
  if (sum > 0.0f) {
    wHue /= sum;
    wBlack /= sum;
    wWhite /= sum;
  }
  // HSV from triangle weights: Value = wHue + wWhite, Saturation = wHue / Value
  const float value = std::min(std::max(wHue + wWhite, 0.0f), 1.0f);
  const float saturation =
      (value > 1e-6f) ? std::min(std::max(wHue / value, 0.0f), 1.0f) : 0.0f;
  s = (int)(saturation * 100.0f + 0.5f);
  v = (int)(value * 100.0f + 0.5f);
}

//-----------------------------------------------------------------------------

QPointF HexagonalColorWheel::svSquareMarkerPos(float s, float v) const {
  const float S = s / 100.0f;
  const float V = v / 100.0f;
  return QPointF(m_svSquare.left() + S * m_svSquare.width(),
                 m_svSquare.top() + (1.0f - V) * m_svSquare.height());
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::svFromSquarePoint(const QPointF &localPos, int &s,
                                            int &v) const {
  const float w = std::max((float)m_svSquare.width(), 1.0f);
  const float h = std::max((float)m_svSquare.height(), 1.0f);
  float S = (localPos.x() - m_svSquare.left()) / w;
  float V = 1.0f - (localPos.y() - m_svSquare.top()) / h;
  S       = std::min(std::max(S, 0.0f), 1.0f);
  V       = std::min(std::max(V, 0.0f), 1.0f);
  s       = (int)(S * 100.0f + 0.5f);
  v       = (int)(V * 100.0f + 0.5f);
}

//-----------------------------------------------------------------------------

QPointF HexagonalColorWheel::svTriangleMarkerPos(float s, float v) const {
  const float S = s / 100.0f;
  const float V = v / 100.0f;
  const float wHue   = S * V;
  const float wWhite = (1.0f - S) * V;
  const float wBlack = 1.0f - V;
  return wHue * m_leftp[0] + wBlack * m_leftp[1] + wWhite * m_leftp[2];
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawColorCursor(float x, float y) {
  const float dpr = (float)getDevPixRatio();
  const float h   = 3.5f * dpr;
  auto square     = [&](float s) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - s, y - s);
    glVertex2f(x + s, y - s);
    glVertex2f(x + s, y + s);
    glVertex2f(x - s, y + s);
    glEnd();
  };
  glColor3f(0.1f, 0.1f, 0.1f);
  square(h + 1.0f * dpr);
  glColor3f(1.0f, 1.0f, 1.0f);
  square(h);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawHueRingBaton(int hue, float innerDist,
                                           float outerDist,
                                           const QPointF &center) {
  float rad       = (float)hue / 180.0f * 3.1415f;
  float halfAngle = 2.4f / std::max(outerDist, 1.0f);
  float r0        = rad - halfAngle;
  float r1        = rad + halfAngle;
  float cx        = center.x();
  float cy        = center.y();
  auto wedge      = [&](float inR, float outR) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx + inR * cosf(r0), cy - inR * sinf(r0));
    glVertex2f(cx + outR * cosf(r0), cy - outR * sinf(r0));
    glVertex2f(cx + outR * cosf(r1), cy - outR * sinf(r1));
    glVertex2f(cx + inR * cosf(r1), cy - inR * sinf(r1));
    glEnd();
  };
  const float pad = 1.2f * (float)getDevPixRatio();
  glColor3f(0.1f, 0.1f, 0.1f);
  wedge(innerDist - pad, outerDist + pad);
  glColor3f(1.0f, 1.0f, 1.0f);
  wedge(innerDist, outerDist);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawCurrentColorMark() {
  int h = 360 - m_color.getValue(eHue);
  int hue = m_color.getValue(eHue);

  if (m_pageMode == ColorPageMode::Classic) {
    float s   = (float)m_color.getValue(eSaturation) / 100.0f;
    glPushMatrix();
    float phi = (float)(h % 60 - 30) / 180.0f * 3.1415f;
    float d   = s * m_hexTriHeight / cosf(phi);
    glTranslatef(m_wp[0].x(), m_wp[0].y(), 0.1f);
    glRotatef(h, 0.0, 0.0, 1.0);
    glTranslatef(d, 0.0f, 0.0f);
    glRotatef(-h, 0.0, 0.0, 1.0);
    drawColorCursor(0.0f, 0.0f);
    glPopMatrix();
  } else {
    drawHueRingBaton(hue, m_innerRadius, m_outerRadius, m_circleCenter);
  }

  QPointF marker =
      (m_pageMode == ColorPageMode::Advanced &&
       m_svShape == AdvancedSvShape::Square)
          ? svSquareMarkerPos((float)m_color.getValue(eSaturation),
                              (float)m_color.getValue(eValue))
          : svTriangleMarkerPos((float)m_color.getValue(eSaturation),
                                (float)m_color.getValue(eValue));
  glPushMatrix();
  glTranslatef(0.0f, 0.0f, 0.1f);
  drawColorCursor((float)marker.x(), (float)marker.y());
  glPopMatrix();
}

//-----------------------------------------------------------------------------

bool HexagonalColorWheel::isInClassicWheel(const QPoint &pos) const {
  QPolygonF wheelPolygon;
  wheelPolygon << m_wp[1] << m_wp[2] << m_wp[3] << m_wp[4] << m_wp[5]
               << m_wp[6];
  wheelPolygon.translate(m_wheelPosition);
  return wheelPolygon.toPolygon().containsPoint(pos, Qt::OddEvenFill);
}

//-----------------------------------------------------------------------------

bool HexagonalColorWheel::isInCircularHueRing(const QPoint &pos) const {
  QPointF local = QPointF(pos) - m_wheelPosition;
  float dist    = QLineF(m_circleCenter, local).length();
  return dist >= m_innerRadius && dist <= m_outerRadius;
}

//-----------------------------------------------------------------------------

bool HexagonalColorWheel::isInHueRing(const QPoint &pos) const {
  if (isInSvField(pos)) return false;
  return isInCircularHueRing(pos);
}

//-----------------------------------------------------------------------------

bool HexagonalColorWheel::isInTriangle(const QPoint &pos) const {
  QPolygonF triPolygon;
  triPolygon << m_leftp[0] << m_leftp[1] << m_leftp[2];
  triPolygon.translate(m_wheelPosition);
  return triPolygon.toPolygon().containsPoint(pos, Qt::OddEvenFill);
}

//-----------------------------------------------------------------------------

bool HexagonalColorWheel::isInSvSquare(const QPoint &pos) const {
  QPointF local = QPointF(pos) - m_wheelPosition;
  return m_svSquare.contains(local);
}

//-----------------------------------------------------------------------------

bool HexagonalColorWheel::isInSvField(const QPoint &pos) const {
  if (m_pageMode == ColorPageMode::Advanced &&
      m_svShape == AdvancedSvShape::Square)
    return isInSvSquare(pos);
  return isInTriangle(pos);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::mousePressEvent(QMouseEvent *event) {
  if (~event->buttons() & Qt::LeftButton) return;

  QPoint curPos = event->pos() * getDevPixRatio();

  if (isInSvField(curPos)) {
    m_currentWheel = rightTriangle;
    clickSvField(curPos);
    return;
  }

  if (m_pageMode == ColorPageMode::Classic) {
    if (isInClassicWheel(curPos)) {
      m_currentWheel = leftWheel;
      clickLeftWheel(curPos);
      return;
    }
  } else if (isInHueRing(curPos)) {
    m_currentWheel = leftWheel;
    clickHueRing(curPos);
    return;
  }

  m_currentWheel = none;
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::mouseMoveEvent(QMouseEvent *event) {
  // change the behavior according to the current touching wheel
  switch (m_currentWheel) {
  case none:
    break;
  case leftWheel:
    if (m_pageMode == ColorPageMode::Classic)
      clickLeftWheel(event->pos() * getDevPixRatio());
    else
      clickHueRing(event->pos() * getDevPixRatio());
    break;
  case rightTriangle:
    clickSvField(event->pos() * getDevPixRatio());
    break;
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::mouseReleaseEvent(QMouseEvent *event) {
  m_currentWheel = none;
  emit colorChanged(m_color, false);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::clickHueRing(const QPoint &pos) {
  QPointF center = m_circleCenter + m_wheelPosition;
  QPointF d      = QPointF(pos) - center;
  float theta    = atan2f(-d.y(), d.x()) * 180.0f / 3.1415f;
  if (theta < 0.0f) theta += 360.0f;
  int hue = (int)(theta + 0.5f);
  if (hue > 359) hue = 359;

  m_color.setValue(eHue, hue);
  emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

/*! compute hue and saturation position. saturation value must be clamped
 */
void HexagonalColorWheel::clickLeftWheel(const QPoint &pos) {
  QLineF p(m_wp[0] + m_wheelPosition, QPointF(pos));
  QLineF horizontal(0, 0, 1, 0);
  float theta =
      (p.dy() >= 0) ? horizontal.angleTo(p) : 360 - p.angleTo(horizontal);
  float phi = theta;
  while (phi >= 60.0f) phi -= 60.0f;
  phi -= 30.0f;
  // d is a length from center to edge of the wheel when saturation = 100
  float d = m_hexTriHeight / cosf(phi / 180.0f * 3.1415f);

  int h = (int)theta;
  if (h > 359) h = 359;
  // clamping
  int s = (int)(std::min(p.length() / d, 1.0) * 100.0f);

  m_color.setValues(eValue, h, s);

  emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::clickRightTriangle(const QPoint &pos) {
  QPointF local = QPointF(pos) - m_wheelPosition;
  int s, v;
  svFromTrianglePoint(local, s, v);
  m_color.setValue(eSaturation, s);
  m_color.setValue(eValue, v);
  emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::clickSvField(const QPoint &pos) {
  if (m_pageMode == ColorPageMode::Advanced &&
      m_svShape == AdvancedSvShape::Square) {
    QPointF local = QPointF(pos) - m_wheelPosition;
    int s, v;
    svFromSquarePoint(local, s, v);
    m_color.setValue(eSaturation, s);
    m_color.setValue(eValue, v);
    emit colorChanged(m_color, true);
    return;
  }
  clickRightTriangle(pos);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::onContextAboutToBeDestroyed() {
  if (!m_lutCalibrator) return;
  makeCurrent();
  m_lutCalibrator->cleanup();
  doneCurrent();
  disconnect(context(), SIGNAL(aboutToBeDestroyed()), this,
             SLOT(onContextAboutToBeDestroyed()));
}

//*****************************************************************************
//    SquaredColorWheel  implementation
//*****************************************************************************

SquaredColorWheel::SquaredColorWheel(QWidget *parent)
    : QWidget(parent), m_channel(eRed), m_color() {}

//-----------------------------------------------------------------------------

void SquaredColorWheel::paintEvent(QPaintEvent *) {
  QPainter p(this);
  int w = width();
  int h = height();
  if (w < 2 || h < 2) return;

  QPixmap bgPixmap = makeSquareShading(m_color, m_channel, w, h);

  if (!bgPixmap.isNull()) p.drawPixmap(0, 0, w, h, bgPixmap);

  int u = 0, v = 0;
  m_color.getValues(m_channel, u, v);
  int x = u * width() / ChannelPairMaxValues[m_channel][0];
  int y = (ChannelPairMaxValues[m_channel][1] - v) * height() /
          ChannelPairMaxValues[m_channel][1];

  p.setPen(QPen(QColor(26, 26, 26), 1));
  p.drawRect(QRectF(x - 4.5, y - 4.5, 9.0, 9.0));
  p.setPen(QPen(Qt::white, 1));
  p.drawRect(QRectF(x - 3.5, y - 3.5, 7.0, 7.0));
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::click(const QPoint &pos) {
  int u = ChannelPairMaxValues[m_channel][0] * pos.x() / width();
  int v = ChannelPairMaxValues[m_channel][1] * (height() - pos.y()) / height();
  u     = tcrop(u, 0, ChannelPairMaxValues[m_channel][0]);
  v     = tcrop(v, 0, ChannelPairMaxValues[m_channel][1]);
  m_color.setValues(m_channel, u, v);
  update();
  emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::mousePressEvent(QMouseEvent *event) {
  if (event->buttons() & Qt::LeftButton) click(event->pos());
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() & Qt::LeftButton) click(event->pos());
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::mouseReleaseEvent(QMouseEvent *event) {
  emit colorChanged(m_color, false);
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::setColor(const ColorModel &color) { m_color = color; }

//-----------------------------------------------------------------------------

void SquaredColorWheel::setChannel(int channel) {
  assert(0 <= channel && channel < 7);
  m_channel = (ColorChannel)channel;
  update();
}

//*****************************************************************************
//    ColorSlider  implementation
//*****************************************************************************

// Acquire size later...
int ColorSlider::s_chandle_size      = -1;
int ColorSlider::s_chandle_tall      = -1;
int ColorSlider::s_slider_appearance = -1;

ColorSlider::ColorSlider(Qt::Orientation orientation, QWidget *parent)
    : QAbstractSlider(parent), m_channel(eRed), m_color() {
  setFocusPolicy(Qt::NoFocus);

  setOrientation(orientation);
  setMinimum(0);
  setMaximum(ChannelMaxValues[m_channel]);

  setMinimumHeight(7);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  // Get color handle size once
  if (s_chandle_size == -1) {
    QImage chandle      = QImage(":Resources/h_chandle_arrow.svg");
    s_chandle_size      = chandle.width();
    s_chandle_tall      = chandle.height();
    s_slider_appearance = StyleEditorColorSliderAppearance;
  }

  // Warning: necessary to identify the object in the style definition file
  setObjectName("colorSlider");
}

//-----------------------------------------------------------------------------

void ColorSlider::setChannel(ColorChannel channel) {
  if (m_channel == channel) return;
  m_channel = channel;
  setMaximum(ChannelMaxValues[m_channel]);
}

//-----------------------------------------------------------------------------

void ColorSlider::setColor(const ColorModel &color) { m_color = color; }

//-----------------------------------------------------------------------------

void ColorSlider::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  int x = rect().x();
  int y = rect().y();
  int w = width();
  int h = height();

  bool isVertical = orientation() == Qt::Vertical;
  bool isLineHandle =
      ColorSlider::s_slider_appearance == AbsoluteColoredLineHandle;

  if (isVertical) {
    y += s_chandle_size / 2;
    h -= s_chandle_size;
    w -= 3;
  } else {
    x += s_chandle_size / 2;
    w -= s_chandle_size;
    h -= 3;
    if (isLineHandle) {
      y += 1;
      h -= 2;
    }
  }
  if (w < 2 || h < 2) return;

  QPixmap bgPixmap =
      makeLinearShading(m_color, m_channel, isVertical ? h : w, isVertical);

  if (m_channel == eAlpha) {
    p.drawTiledPixmap(x, y, w, h,
                      DVGui::CommonChessboard::instance()->getPixmap());
  }

  if (!bgPixmap.isNull()) {
    p.drawTiledPixmap(x, y, w, h, bgPixmap);
  }

  /*!
     Bug in Qt 4.3: The vertical Slider cannot be styled due to a bug.
     In this case we draw "manually" the slider handle at correct position
  */
  if (isVertical) {
    static QPixmap vHandlePixmap =
        svgToPixmap(":Resources/v_chandle_arrow.svg");
    int pos = QStyle::sliderPositionFromValue(0, maximum(), value(), h, true);
    p.drawPixmap(width() - s_chandle_tall, pos, vHandlePixmap);
  } else {
    int pos = QStyle::sliderPositionFromValue(0, maximum(), value(), w, false);
    if (isLineHandle) {
      static QPixmap hHandleUpPm(":Resources/h_chandleUp.png");
      static QPixmap hHandleDownPm(":Resources/h_chandleDown.png");
      static QPixmap hHandleCenterPm(":Resources/h_chandleCenter.png");
      int linePos = pos + (s_chandle_size - hHandleCenterPm.width()) / 2;
      p.drawPixmap(linePos, 0, hHandleUpPm);
      p.drawPixmap(linePos, height() - hHandleDownPm.height(), hHandleDownPm);
      p.drawPixmap(linePos, hHandleUpPm.height(), hHandleCenterPm.width(),
                   height() - hHandleUpPm.height() - hHandleDownPm.height(),
                   hHandleCenterPm);
    } else {
      static QPixmap hHandlePixmap =
          svgToPixmap(":Resources/h_chandle_arrow.svg");
      p.drawPixmap(pos, height() - s_chandle_tall, hHandlePixmap);
    }
  }
};

//-----------------------------------------------------------------------------

void ColorSlider::mousePressEvent(QMouseEvent *event) {
  chandleMouse(event->pos().x(), event->pos().y());
}

//-----------------------------------------------------------------------------

void ColorSlider::mouseReleaseEvent(QMouseEvent *event) {
  emit sliderReleased();
}

//-----------------------------------------------------------------------------

void ColorSlider::mouseMoveEvent(QMouseEvent *event) {
  chandleMouse(event->pos().x(), event->pos().y());
}

//-----------------------------------------------------------------------------

void ColorSlider::chandleMouse(int mouse_x, int mouse_y) {
  if (orientation() == Qt::Vertical) {
    int pos  = mouse_y - s_chandle_size / 2;
    int span = height() - s_chandle_size;
    setValue(QStyle::sliderValueFromPosition(0, maximum(), pos, span, true));
  } else {
    int pos  = mouse_x - s_chandle_size / 2;
    int span = width() - s_chandle_size;
    setValue(QStyle::sliderValueFromPosition(0, maximum(), pos, span, false));
  }
}

//*****************************************************************************
//    ArrowButton  implementation
//*****************************************************************************

ArrowButton::ArrowButton(QWidget *parent, Qt::Orientation orientation,
                         bool isFirstArrow)
    : QToolButton(parent)
    , m_orientaion(orientation)
    , m_isFirstArrow(isFirstArrow)
    , m_timerId(0)
    , m_firstTimerId(0) {
  setFixedSize(10, 10);
  setObjectName("StyleEditorArrowButton");
  bool isVertical = orientation == Qt::Vertical;
  if (m_isFirstArrow) {
    if (isVertical)
      setIcon(createQIconPNG("arrow_up"));
    else
      setIcon(createQIconPNG("arrow_left"));
  } else {
    if (isVertical)
      setIcon(createQIconPNG("arrow_down"));
    else
      setIcon(createQIconPNG("arrow_right"));
  }
  connect(this, SIGNAL(pressed()), this, SLOT(onPressed()));
  connect(this, SIGNAL(released()), this, SLOT(onRelease()));
}

//-----------------------------------------------------------------------------

void ArrowButton::timerEvent(QTimerEvent *event) {
  if (m_firstTimerId != 0) {
    killTimer(m_firstTimerId);
    m_firstTimerId = 0;
    m_timerId      = startTimer(10);
  }
  notifyChanged();
}

//-----------------------------------------------------------------------------

void ArrowButton::notifyChanged() {
  bool isVertical = m_orientaion == Qt::Vertical;
  if ((m_isFirstArrow && !isVertical) || (!m_isFirstArrow && isVertical))
    emit remove();
  else
    emit add();
}

//-----------------------------------------------------------------------------

void ArrowButton::onPressed() {
  notifyChanged();
  assert(m_timerId == 0 && m_firstTimerId == 0);
  m_firstTimerId = startTimer(500);
}

//-----------------------------------------------------------------------------

void ArrowButton::onRelease() {
  if (m_firstTimerId != 0) {
    killTimer(m_firstTimerId);
    m_firstTimerId = 0;
  } else if (m_timerId != 0) {
    killTimer(m_timerId);
    m_timerId = 0;
  }
}

//*****************************************************************************
//    ColorSliderBar  implementation
//*****************************************************************************

ColorSliderBar::ColorSliderBar(QWidget *parent, Qt::Orientation orientation)
    : QWidget(parent) {
  bool isVertical = orientation == Qt::Vertical;

  ArrowButton *first = new ArrowButton(this, orientation, true);
  connect(first, SIGNAL(remove()), this, SLOT(onRemove()));
  connect(first, SIGNAL(add()), this, SLOT(onAdd()));

  m_colorSlider = new ColorSlider(orientation, this);
  if (isVertical) {
    m_colorSlider->setMinimumWidth(10);
    m_colorSlider->setMaximumWidth(28);
  }

  ArrowButton *last = new ArrowButton(this, orientation, false);
  connect(last, SIGNAL(add()), this, SLOT(onAdd()));
  connect(last, SIGNAL(remove()), this, SLOT(onRemove()));

  connect(m_colorSlider, SIGNAL(valueChanged(int)), this,
          SIGNAL(valueChanged(int)));
  connect(m_colorSlider, SIGNAL(sliderReleased()), this,
          SIGNAL(valueChanged()));

  QBoxLayout *layout;
  if (!isVertical)
    layout = new QHBoxLayout(this);
  else
    layout = new QVBoxLayout(this);

  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(first, 0, Qt::AlignCenter);
  layout->addWidget(m_colorSlider, 1);
  layout->addWidget(last, 0, Qt::AlignCenter);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

void ColorSliderBar::onRemove() {
  int value = m_colorSlider->value();
  if (value <= m_colorSlider->minimum()) return;
  m_colorSlider->setValue(value - 1);
}

//-----------------------------------------------------------------------------

void ColorSliderBar::onAdd() {
  int value = m_colorSlider->value();
  if (value >= m_colorSlider->maximum()) return;
  m_colorSlider->setValue(value + 1);
}

//*****************************************************************************
//    ChannelLineEdit  implementation
//*****************************************************************************

void ChannelLineEdit::mousePressEvent(QMouseEvent *e) {
  IntLineEdit::mousePressEvent(e);

  if (!m_isEditing) {
    selectAll();
    m_isEditing = true;
  }
}

//-----------------------------------------------------------------------------

void ChannelLineEdit::focusOutEvent(QFocusEvent *e) {
  IntLineEdit::focusOutEvent(e);

  m_isEditing = false;
}

//-----------------------------------------------------------------------------

void ChannelLineEdit::paintEvent(QPaintEvent *e) {
  IntLineEdit::paintEvent(e);

  /* Now that stylesheets added lineEdit focus this is likely redundant,
   * commenting out in-case it is still required.
  if (m_isEditing) {
    QPainter p(this);
    p.setPen(Qt::yellow);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
  }
  */
}

//*****************************************************************************
//    ColorChannelControl  implementation
//*****************************************************************************

namespace {
int channelRadioColumnWidth() {
  static int w = 0;
  if (w <= 0) {
    QRadioButton probe;
    w = probe.sizeHint().width();
    if (w < 13) w = 16;
  }
  return w;
}
}  // namespace

ColorChannelControl::ColorChannelControl(ColorChannel channel, QWidget *parent)
    : QWidget(parent)
    , m_modeRadio(0)
    , m_radioSlot(0)
    , m_channel(channel)
    , m_value(0)
    , m_signalEnabled(true) {
  setFocusPolicy(Qt::NoFocus);

  QStringList channelList;
  channelList << tr("R") << tr("G") << tr("B") << tr("A") << tr("H") << tr("S")
              << tr("V");
  assert(0 <= (int)m_channel && (int)m_channel < 7);
  QString text = channelList.at(m_channel);
  m_label      = new QLabel(text, this);

  int minValue = 0;
  int maxValue = 0;
  if (m_channel < 4)  // RGBA
    maxValue = 255;
  else if (m_channel == 4)  // H
    maxValue = 359;
  else  // SV
    maxValue = 100;

  m_field  = new ChannelLineEdit(this, 0, minValue, maxValue);
  m_slider = new ColorSlider(Qt::Horizontal, this);

  m_radioSlot = new QWidget(this);
  m_radioSlot->setFixedWidth(0);
  QHBoxLayout *radioLay = new QHBoxLayout(m_radioSlot);
  radioLay->setContentsMargins(0, 0, 0, 0);
  radioLay->setSpacing(0);
  m_modeRadio = 0;
  if (m_channel != eAlpha) {
    m_modeRadio = new QRadioButton(m_radioSlot);
    m_modeRadio->setFocusPolicy(Qt::NoFocus);
    radioLay->addWidget(m_modeRadio);
  }

  // buttons to increment/decrement the values by 1
  QPushButton *addButton = new QPushButton(this);
  QPushButton *subButton = new QPushButton(this);

  m_slider->setValue(0);
  m_slider->setChannel(m_channel);

  m_label->setObjectName("colorSliderLabel");
  m_label->setFixedWidth(11);
  m_label->setMinimumHeight(7);
  m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

  m_field->setObjectName("colorSliderField");
  m_field->setFixedWidth(fontMetrics().horizontalAdvance('0') * 4);
  m_field->setMinimumHeight(7);

  addButton->setObjectName("colorSliderAddButton");
  subButton->setObjectName("colorSliderSubButton");
  addButton->setFixedWidth(18);
  subButton->setFixedWidth(18);
  addButton->setMinimumHeight(7);
  subButton->setMinimumHeight(7);
  addButton->setFlat(true);
  subButton->setFlat(true);
  addButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  subButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  addButton->setAutoRepeat(true);
  subButton->setAutoRepeat(true);
  addButton->setAutoRepeatInterval(25);
  subButton->setAutoRepeatInterval(25);
  addButton->setFocusPolicy(Qt::NoFocus);
  subButton->setFocusPolicy(Qt::NoFocus);

  QHBoxLayout *mainLayout = new QHBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(1);
  {
    mainLayout->addWidget(m_radioSlot, 0);
    mainLayout->addWidget(m_label, 0);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(m_field, 0);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(subButton, 0);
    mainLayout->addWidget(m_slider, 1);
    mainLayout->addWidget(addButton, 0);
  }
  setLayout(mainLayout);

  bool ret =
      connect(m_field, SIGNAL(editingFinished()), this, SLOT(onFieldChanged()));
  ret = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                       SLOT(onSliderChanged(int)));
  ret = ret && connect(m_slider, SIGNAL(sliderReleased()), this,
                       SLOT(onSliderReleased()));
  ret = ret &&
        connect(addButton, SIGNAL(clicked()), this, SLOT(onAddButtonClicked()));
  ret = ret &&
        connect(subButton, SIGNAL(clicked()), this, SLOT(onSubButtonClicked()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void ColorChannelControl::setModeRadioVisible(bool on) {
  if (m_radioSlot) m_radioSlot->setFixedWidth(on ? channelRadioColumnWidth() : 0);
  if (m_modeRadio) m_modeRadio->setVisible(on);
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onAddButtonClicked() {
  m_slider->setValue(m_slider->value() + 1);
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onSubButtonClicked() {
  m_slider->setValue(m_slider->value() - 1);
}

//-----------------------------------------------------------------------------

void ColorChannelControl::setColor(const ColorModel &color) {
  m_color = color;
  m_slider->setColor(color);
  int value = color.getValue(m_channel);
  if (m_value != value) {
    bool signalEnabled = m_signalEnabled;
    m_signalEnabled    = false;
    m_value            = value;
    m_field->setText(QString::number(value));
    m_slider->setValue(value);
    m_signalEnabled = signalEnabled;
  }
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onFieldChanged() {
  int value = m_field->text().toInt();
  if (m_value == value) return;
  m_value = value;
  m_slider->setValue(value);
  if (m_signalEnabled) {
    m_color.setValue(m_channel, value);
    emit colorChanged(m_color, false);
  }
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onSliderChanged(int value) {
  if (m_value == value) return;
  m_value = value;
  m_field->setText(QString::number(value));
  if (m_signalEnabled) {
    m_color.setValue(m_channel, value);
    emit colorChanged(m_color, true);
  }
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onSliderReleased() {
  emit colorChanged(m_color, false);
}

//*****************************************************************************
//    StyleEditorPage  implementation
//*****************************************************************************

StyleEditorPage::StyleEditorPage(QWidget *parent) : QFrame(parent) {
  setFocusPolicy(Qt::NoFocus);

  // It is necessary for the style sheets
  setObjectName("styleEditorPage");
  setFrameStyle(QFrame::StyledPanel);
}

//*****************************************************************************
//    ColorParameterSelector  implementation
//*****************************************************************************

ColorParameterSelector::ColorParameterSelector(QWidget *parent)
    : QWidget(parent)
    , m_index(0)
    , m_chipSize(21, 21)
    , m_chipOrigin(0, 1)
    , m_chipDelta(21, 0) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::paintEvent(QPaintEvent *event) {
  if (m_colors.empty()) return;
  QPainter p(this);
  int i;
  QRect currentChipRect = QRect();
  for (i = 0; i < (int)m_colors.size(); i++) {
    QRect chipRect(m_chipOrigin + i * m_chipDelta, m_chipSize);
    p.fillRect(chipRect, m_colors[i]);
    if (i == m_index) currentChipRect = chipRect;
  }
  // Current index border
  if (!currentChipRect.isEmpty()) {
    p.setPen(QColor(199, 202, 50));
    p.drawRect(currentChipRect.adjusted(0, 0, -1, -1));
    p.setPen(Qt::white);
    p.drawRect(currentChipRect.adjusted(1, 1, -2, -2));
    p.setPen(Qt::black);
    p.drawRect(currentChipRect.adjusted(2, 2, -3, -3));
  }
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::setStyle(const TColorStyle &style) {
  int count = style.getColorParamCount();
  if (count <= 1) {
    clear();
    return;
  }
  show();
  if (m_colors.size() != count) {
    m_index = 0;
    m_colors.resize(count);
  }
  int i;
  for (i = 0; i < count; i++) {
    TPixel32 color = style.getColorParamValue(i);
    m_colors[i]    = QColor(color.r, color.g, color.b, color.m);
  }
  update();
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::clear() {
  if (m_colors.size() != 0) m_colors.clear();
  m_index = 0;
  if (isVisible()) {
    hide();
    update();
    qApp->processEvents();
  }
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::mousePressEvent(QMouseEvent *event) {
  QPoint pos = event->pos() - m_chipOrigin;
  int index  = pos.x() / m_chipDelta.x();
  QRect chipRect(index * m_chipDelta, m_chipSize);
  if (chipRect.contains(pos)) {
    if (index < m_colors.size()) m_index = index;
    emit colorParamChanged();
    update();
  }
}

//-----------------------------------------------------------------------------

QSize ColorParameterSelector::sizeHint() const {
  return QSize(m_chipOrigin.x() + (m_colors.size() - 1) * m_chipDelta.x() +
                   m_chipSize.width(),
               m_chipOrigin.y() + m_chipSize.height());
}

//*****************************************************************************
//    RectanglePickerPane  implementation
//*****************************************************************************

class RectanglePickerPane final : public QWidget {
public:
  SquaredColorWheel *square;
  ColorSliderBar *slider;
  int gap;
  int barWidth;

  RectanglePickerPane(QWidget *parent)
      : QWidget(parent)
      , square(0)
      , slider(0)
      , gap(3)
      , barWidth(26) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);
  }

  QSize sizeHint() const override { return QSize(120, 120); }
  QSize minimumSizeHint() const override { return QSize(0, 0); }

  void relayout() {
    if (!square || !slider) return;
    const int kBarMin = 14;
    const int kBarMax = 26;
    int barW   = qBound(kBarMin, (width() - gap) / 7, kBarMax);
    int fieldW = qMax(0, width() - barW - gap);
    int fieldH = qMax(0, height());
    square->setGeometry(0, 0, fieldW, fieldH);
    slider->setMinimumWidth(kBarMin);
    slider->setMaximumWidth(kBarMax);
    slider->setGeometry(fieldW + gap, 0, barW, fieldH);
  }

protected:
  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    relayout();
  }
  void showEvent(QShowEvent *e) override {
    QWidget::showEvent(e);
    relayout();
    QTimer::singleShot(0, this, [this]() { relayout(); });
  }
};

//-----------------------------------------------------------------------------

class ColorVariationStrip final : public QWidget {
  static const int kCount  = 9;
  static const int kGap    = 1;
  static const int kMinChip = 8;
  QToolButton *m_chips[kCount];
  ColorModel m_src;
  std::function<void(const ColorModel &)> m_pick;
  int m_rows = 1;

  ColorModel colorAt(int i) const {
    ColorModel cm = m_src;
    const int v   = 14 + i * (100 - 14) / (kCount - 1);
    cm.setValue(eValue, v);
    return cm;
  }

  void paintChip(int i) {
    const TPixel32 p = colorAt(i).getTPixel();
    const QColor qc(p.r, p.g, p.b);
    m_chips[i]->setStyleSheet(
        QStringLiteral("QToolButton { background: %1; border: 1px solid "
                       "palette(mid); padding: 0px; margin: 0px; "
                       "min-width: 0px; min-height: 0px; }")
            .arg(qc.name()));
  }

  int rowCountForWidth(int w) const {
    if (w <= 0) return 1;
    const int oneRow = (w - kGap * (kCount - 1)) / kCount;
    return (oneRow < kMinChip) ? 2 : 1;
  }

  int stripHeight(int rows) const { return rows == 2 ? 28 : 16; }

  void relayout() {
    const int w    = width();
    const int rows = rowCountForWidth(w);
    const int cols = (kCount + rows - 1) / rows;
    const int chipH =
        std::max(8, (stripHeight(rows) - kGap * (rows - 1)) / rows);
    const int chipW =
        std::max(0, (w - kGap * (cols - 1)) / std::max(cols, 1));
    for (int i = 0; i < kCount; ++i) {
      const int r = i / cols;
      const int c = i % cols;
      m_chips[i]->setGeometry(c * (chipW + kGap), r * (chipH + kGap), chipW,
                              chipH);
    }
    if (rows != m_rows) {
      m_rows = rows;
      updateGeometry();
      if (parentWidget()) parentWidget()->updateGeometry();
    }
  }

protected:
  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    relayout();
  }

public:
  explicit ColorVariationStrip(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 16);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    for (int i = 0; i < kCount; ++i) {
      m_chips[i] = new QToolButton(this);
      m_chips[i]->setMinimumSize(0, 0);
      m_chips[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
      m_chips[i]->setFocusPolicy(Qt::NoFocus);
      m_chips[i]->setAutoRaise(false);
      connect(m_chips[i], &QToolButton::clicked, this, [this, i]() {
        if (m_pick) m_pick(colorAt(i));
      });
    }
  }

  QSize sizeHint() const override {
    return QSize(0, stripHeight(rowCountForWidth(width())));
  }
  QSize minimumSizeHint() const override { return QSize(0, 16); }

  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }

  void setFrom(const ColorModel &color) {
    m_src = color;
    for (int i = 0; i < kCount; ++i) paintChip(i);
  }
};

//*****************************************************************************
//    PlainColorPage  implementation
//*****************************************************************************

PlainColorPage::PlainColorPage(QWidget *parent)
    : StyleEditorPage(parent)
    , m_color()
    , m_signalEnabled(true)
    , m_pickerVisible(true)
    , m_pickerSectionAction(0) {
  setFocusPolicy(Qt::NoFocus);
  setMinimumWidth(0);

  m_hexagonalColorWheel = new HexagonalColorWheel(this);
  m_hexagonalColorWheel->setMinimumSize(0, 0);
  m_hexagonalColorWheel->setContextMenuPolicy(Qt::NoContextMenu);
  m_squaredColorWheel   = new SquaredColorWheel(this);
  m_squaredColorWheel->setMinimumSize(0, 0);
  m_verticalSlider      = new ColorSliderBar(this, Qt::Vertical);
  m_channelButtonGroup  = new QButtonGroup(this);
  m_channelButtonGroup->setExclusive(true);

  for (int i = 0; i < 7; i++) {
    m_channelControls[i] = new ColorChannelControl((ColorChannel)i, this);
    m_channelControls[i]->setColor(m_color);
    connect(m_channelControls[i],
            SIGNAL(colorChanged(const ColorModel &, bool)), this,
            SLOT(onControlChanged(const ColorModel &, bool)));
    if (QRadioButton *radio = m_channelControls[i]->modeRadio()) {
      m_channelButtonGroup->addButton(radio, i);
      if (i == (int)eHue) radio->setChecked(true);
    }
  }
  connect(m_channelButtonGroup, SIGNAL(buttonClicked(int)), this,
          SLOT(setWheelChannel(int)));

  m_pickerFrame = new QFrame(this);
  m_swatchFrame = new QFrame(this);
  m_hsvFrame   = new QFrame(this);
  m_alphaFrame = new QFrame(this);
  m_rgbFrame   = new QFrame(this);

  m_slidersContainer = new QFrame(this);
  m_vSplitter        = new QSplitter(this);

  m_pickerFrame->setObjectName("PlainColorPageParts");
  m_swatchFrame->setObjectName("PlainColorPageParts");
  m_hsvFrame->setObjectName("PlainColorPageParts");
  m_alphaFrame->setObjectName("PlainColorPageParts");
  m_rgbFrame->setObjectName("PlainColorPageParts");

  m_vSplitter->setOrientation(Qt::Vertical);
  m_vSplitter->setFocusPolicy(Qt::NoFocus);
  m_vSplitter->setMinimumWidth(0);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setSpacing(0);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  {
    QVBoxLayout *pickerLayout = new QVBoxLayout();
    pickerLayout->setContentsMargins(5, 5, 5, 5);
    pickerLayout->setSpacing(0);

    RectanglePickerPane *rectPane = new RectanglePickerPane(m_pickerFrame);
    m_rectPicker                  = rectPane;
    m_rectPicker->setContextMenuPolicy(Qt::CustomContextMenu);
    m_squaredColorWheel->setParent(rectPane);
    m_verticalSlider->setParent(rectPane);
    m_verticalSlider->setChannel(eHue);
    m_verticalSlider->setRange(0, ChannelMaxValues[eHue]);
    rectPane->square = m_squaredColorWheel;
    rectPane->slider = m_verticalSlider;

    m_hexagonalColorWheel->setParent(m_pickerFrame);
    m_hexagonalColorWheel->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Expanding);
    m_rectPicker->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pickerLayout->addWidget(m_hexagonalColorWheel, 1);
    pickerLayout->addWidget(m_rectPicker, 1);
    m_rectPicker->hide();

    m_pickerFrame->setMinimumWidth(0);
    m_pickerFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_pickerFrame->setLayout(pickerLayout);

    m_svShapeBtn = new QToolButton(m_pickerFrame);
    m_svShapeBtn->setFixedSize(20, 20);
    m_svShapeBtn->setAutoRaise(true);
    m_svShapeBtn->setFocusPolicy(Qt::NoFocus);
    m_svShapeBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_svShapeBtn->setIconSize(QSize(16, 16));
    m_svShapeBtn->setIcon(createQIcon("colorpicker_sv_square"));
    connect(m_svShapeBtn, SIGNAL(clicked()), this, SIGNAL(svShapeClicked()));
    m_pickerFrame->installEventFilter(this);

    m_vSplitter->addWidget(m_pickerFrame);

    QVBoxLayout *slidersLayout = new QVBoxLayout();
    slidersLayout->setContentsMargins(0, 0, 0, 0);
    slidersLayout->setSpacing(0);
    {
      QVBoxLayout *hsvLayout = new QVBoxLayout();
      hsvLayout->setContentsMargins(4, 4, 4, 4);
      hsvLayout->setSpacing(4);
      {
        hsvLayout->addWidget(m_channelControls[eHue]);
        hsvLayout->addWidget(m_channelControls[eSaturation]);
        hsvLayout->addWidget(m_channelControls[eValue]);
      }
      m_hsvFrame->setLayout(hsvLayout);
      slidersLayout->addWidget(m_hsvFrame, 3);

      QVBoxLayout *alphaLayout = new QVBoxLayout();
      alphaLayout->setContentsMargins(4, 4, 4, 4);
      alphaLayout->setSpacing(4);
      { alphaLayout->addWidget(m_channelControls[eAlpha]); }
      m_alphaFrame->setLayout(alphaLayout);
      slidersLayout->addWidget(m_alphaFrame, 1);

      QVBoxLayout *rgbLayout = new QVBoxLayout();
      rgbLayout->setContentsMargins(4, 4, 4, 4);
      rgbLayout->setSpacing(4);
      {
        rgbLayout->addWidget(m_channelControls[eRed]);
        rgbLayout->addWidget(m_channelControls[eGreen]);
        rgbLayout->addWidget(m_channelControls[eBlue]);
      }
      m_rgbFrame->setLayout(rgbLayout);
      slidersLayout->addWidget(m_rgbFrame, 3);
    }
    m_slidersContainer->setLayout(slidersLayout);
    m_slidersContainer->setMinimumWidth(0);
    m_vSplitter->addWidget(m_slidersContainer);

    m_pickerChrome = new QWidget(this);
    m_pickerChrome->setFixedHeight(20);
    m_pickerChrome->setMinimumWidth(0);
    m_pickerChrome->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout *chromeLay = new QHBoxLayout(m_pickerChrome);
    chromeLay->setContentsMargins(4, 0, 4, 0);
    chromeLay->setSpacing(2);
    const QString chromeIconQss =
        QStringLiteral("QToolButton { margin: 0px; padding: 0px; }");

    m_sectionBar = new QWidget(m_pickerChrome);
    m_sectionBar->setMinimumWidth(0);
    m_sectionBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    QHBoxLayout *secLay = new QHBoxLayout(m_sectionBar);
    secLay->setContentsMargins(0, 0, 0, 0);
    secLay->setSpacing(1);

    m_advancedModeBtn = new QToolButton(m_pickerChrome);
    m_advancedModeBtn->setCheckable(true);
    m_advancedModeBtn->setFixedSize(20, 20);
    m_advancedModeBtn->setAutoRaise(true);
    m_advancedModeBtn->setFocusPolicy(Qt::NoFocus);
    m_advancedModeBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_advancedModeBtn->setIconSize(QSize(16, 16));
    m_advancedModeBtn->setStyleSheet(chromeIconQss);
    m_advancedModeBtn->setIcon(createQIcon("colorpicker_advanced"));
    connect(m_advancedModeBtn, SIGNAL(clicked()), this,
            SIGNAL(colorPageModeClicked()));

    m_wheelKindBtn = new QToolButton(m_pickerChrome);
    m_wheelKindBtn->setCheckable(true);
    m_wheelKindBtn->setFixedSize(20, 20);
    m_wheelKindBtn->setAutoRaise(true);
    m_wheelKindBtn->setFocusPolicy(Qt::NoFocus);
    m_wheelKindBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_wheelKindBtn->setIconSize(QSize(14, 14));
    m_wheelKindBtn->setStyleSheet(chromeIconQss);
    m_wheelKindBtn->setIcon(createQIcon("colorpicker_wheel"));
    m_wheelKindBtn->setToolTip(tr("Wheel"));
    m_wheelKindBtn->setProperty("kind", (int)AdvancedPickerKind::Wheel);
    connect(m_wheelKindBtn, SIGNAL(clicked()), this,
            SLOT(onPickerKindButtonClicked()));

    m_rectKindBtn = new QToolButton(m_pickerChrome);
    m_rectKindBtn->setCheckable(true);
    m_rectKindBtn->setFixedSize(20, 20);
    m_rectKindBtn->setAutoRaise(true);
    m_rectKindBtn->setFocusPolicy(Qt::NoFocus);
    m_rectKindBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_rectKindBtn->setIconSize(QSize(14, 14));
    m_rectKindBtn->setStyleSheet(chromeIconQss);
    m_rectKindBtn->setIcon(createQIcon("colorpicker_rectangle"));
    m_rectKindBtn->setToolTip(tr("Rectangle"));
    m_rectKindBtn->setProperty("kind", (int)AdvancedPickerKind::Rectangle);
    connect(m_rectKindBtn, SIGNAL(clicked()), this,
            SLOT(onPickerKindButtonClicked()));

    chromeLay->addWidget(m_sectionBar, 0);
    chromeLay->addStretch(1);
    chromeLay->addWidget(m_wheelKindBtn, 0, Qt::AlignVCenter);
    chromeLay->addWidget(m_rectKindBtn, 0, Qt::AlignVCenter);
    chromeLay->addWidget(m_advancedModeBtn, 0, Qt::AlignVCenter);

    mainLayout->addWidget(m_pickerChrome, 0);

    m_variationStrip = new ColorVariationStrip(m_swatchFrame);
    static_cast<ColorVariationStrip *>(m_variationStrip)
        ->setPick([this](const ColorModel &c) {
          if (!(m_color == c)) {
            m_color = c;
            updateControls();
          }
          if (m_signalEnabled) emit colorChanged(m_color, false);
        });
    QVBoxLayout *swatchLay = new QVBoxLayout(m_swatchFrame);
    swatchLay->setContentsMargins(4, 2, 4, 2);
    swatchLay->setSpacing(0);
    swatchLay->addWidget(m_variationStrip);
    m_swatchFrame->setMinimumSize(0, 0);
    m_swatchFrame->setMaximumHeight(36);
    m_swatchFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_swatchFrame->hide();
    mainLayout->addWidget(m_swatchFrame, 0);

    mainLayout->addWidget(m_vSplitter, 1);
  }
  setLayout(mainLayout);

  QList<int> list;
  list << rect().height() / 2 << rect().height() / 2;
  m_vSplitter->setSizes(list);

  connect(m_hexagonalColorWheel, SIGNAL(colorChanged(const ColorModel &, bool)),
          this, SLOT(onWheelChanged(const ColorModel &, bool)));
  connect(m_squaredColorWheel, SIGNAL(colorChanged(const ColorModel &, bool)),
          this, SLOT(onWheelChanged(const ColorModel &, bool)));
  connect(m_verticalSlider, SIGNAL(valueChanged(int)), this,
          SLOT(onWheelSliderChanged(int)));
  connect(m_verticalSlider, SIGNAL(valueChanged()), this,
          SLOT(onWheelSliderReleased()));
  connect(m_rectPicker, SIGNAL(customContextMenuRequested(QPoint)), this,
          SLOT(onRectPickerContextMenu(QPoint)));

  auto enableCtx = [this](QWidget *w) {
    if (!w) return;
    w->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(w, SIGNAL(customContextMenuRequested(QPoint)), this,
            SLOT(onPageContextMenu(QPoint)));
  };
  enableCtx(this);
  enableCtx(m_pickerFrame);
  enableCtx(m_slidersContainer);
  enableCtx(m_hsvFrame);
  enableCtx(m_alphaFrame);
  enableCtx(m_rgbFrame);
  enableCtx(m_vSplitter);
  enableCtx(m_pickerChrome);
  enableCtx(m_sectionBar);
  m_pickerChrome->installEventFilter(this);
  enableCtx(m_swatchFrame);

  m_squaredColorWheel->setChannel(eHue);
  updatePickerChrome();
}

//-----------------------------------------------------------------------------

void PlainColorPage::resizeEvent(QResizeEvent *) {
  fitPickerChrome();
  placeSvShapeButton();
}

//-----------------------------------------------------------------------------

void PlainColorPage::showEvent(QShowEvent *e) {
  StyleEditorPage::showEvent(e);
  placeSvShapeButton();
  QTimer::singleShot(0, this, SLOT(refreshPickerLayout()));
}

//-----------------------------------------------------------------------------

bool PlainColorPage::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_pickerChrome && event->type() == QEvent::Resize)
    fitPickerChrome();
  if (watched == m_pickerFrame && (event->type() == QEvent::Resize ||
                                  event->type() == QEvent::Show))
    placeSvShapeButton();
  return QFrame::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------

void PlainColorPage::placeSvShapeButton() {
  if (!m_svShapeBtn || !m_pickerFrame || !m_svShapeBtn->isVisible()) return;
  const int m = 2;
  const int s = m_svShapeBtn->width();
  m_svShapeBtn->move(m_pickerFrame->width() - s - m,
                     m_pickerFrame->height() - s - m);
  m_svShapeBtn->raise();
}

//-----------------------------------------------------------------------------

void PlainColorPage::fitPickerChrome() {
  if (m_fittingChrome || !m_pickerChrome || !m_sectionBar) return;
  QHBoxLayout *chromeLay =
      qobject_cast<QHBoxLayout *>(m_pickerChrome->layout());
  QHBoxLayout *secLay = qobject_cast<QHBoxLayout *>(m_sectionBar->layout());
  if (!chromeLay || !secLay) return;

  m_fittingChrome = true;

  const int kFont     = 10;
  const int kPad      = 2;
  const int kIcon     = 20;
  const int kAdvIcon  = 16;
  const int kKindIcon = 14;
  const int kChromeM  = 4;
  const int kChromeSp = 2;
  const int kSecSp    = 1;

  QList<QToolButton *> textBtns;
  for (QToolButton *btn : m_sectionBar->findChildren<QToolButton *>()) {
    if (!btn->isHidden()) textBtns.append(btn);
  }
  QList<QToolButton *> iconBtns;
  if (m_wheelKindBtn && !m_wheelKindBtn->isHidden())
    iconBtns.append(m_wheelKindBtn);
  if (m_rectKindBtn && !m_rectKindBtn->isHidden())
    iconBtns.append(m_rectKindBtn);
  if (m_advancedModeBtn && !m_advancedModeBtn->isHidden())
    iconBtns.append(m_advancedModeBtn);

  auto apply = [&](double scale) {
    const int fontPx =
        std::max(6, (int)std::lround((double)kFont * scale));
    const int pad = std::max(0, (int)std::lround((double)kPad * scale));
    const int iconW =
        std::max(12, (int)std::lround((double)kIcon * scale));
    const int advIcon =
        std::max(8, (int)std::lround((double)kAdvIcon * scale));
    const int kindIcon =
        std::max(8, (int)std::lround((double)kKindIcon * scale));
    const int chromeM =
        std::max(1, (int)std::lround((double)kChromeM * scale));
    const int chromeSp =
        std::max(0, (int)std::lround((double)kChromeSp * scale));
    const int secSp = std::max(0, (int)std::lround((double)kSecSp * scale));

    chromeLay->setContentsMargins(chromeM, 0, chromeM, 0);
    chromeLay->setSpacing(chromeSp);
    secLay->setSpacing(secSp);

    QFont tf = m_pickerChrome->font();
    tf.setPixelSize(fontPx);
    const QString qss = QStringLiteral(
        "QToolButton { font-size: %1px; padding: 0px %2px; margin: 0px; "
        "min-width: 0px; min-height: 0px; }"
        "QToolButton:hover, QToolButton:checked, QToolButton:checked:hover { "
        "padding: 0px %2px; margin: 0px; }")
                            .arg(fontPx)
                            .arg(pad);
    QFontMetrics fm(tf);
    for (QToolButton *btn : textBtns) {
      btn->setFont(tf);
      btn->setStyleSheet(qss);
      btn->setFixedHeight(20);
      const int w = fm.horizontalAdvance(btn->text()) + pad * 2 + 2;
      if (scale >= 0.999) {
        btn->setMinimumWidth(0);
        btn->setMaximumWidth(QWIDGETSIZE_MAX);
      } else {
        btn->setFixedWidth(std::max(w, 8));
      }
    }
    for (QToolButton *btn : iconBtns) {
      btn->setFixedSize(iconW, 20);
      const int isz = (btn == m_advancedModeBtn) ? advIcon : kindIcon;
      btn->setIconSize(QSize(isz, isz));
    }
  };

  apply(1.0);
  chromeLay->activate();

  int nLay = 0;
  int need = chromeLay->contentsMargins().left() +
             chromeLay->contentsMargins().right();
  for (int i = 0; i < chromeLay->count(); ++i) {
    QLayoutItem *it = chromeLay->itemAt(i);
    if (!it) continue;
    if (it->spacerItem()) {
      ++nLay;
      continue;
    }
    QWidget *w = it->widget();
    if (!w || w->isHidden()) continue;
    need += w->sizeHint().width();
    ++nLay;
  }
  if (nLay > 1) need += chromeLay->spacing() * (nLay - 1);

  const int avail = m_pickerChrome->width();
  double scale    = 1.0;
  if (avail > 0 && need > avail)
    scale = std::max(0.6, (double)avail / (double)need);
  if (scale < 0.999) apply(scale);

  m_fittingChrome = false;
}

//-----------------------------------------------------------------------------

void PlainColorPage::updateControls() {
  int i;
  for (i = 0; i < 7; i++) {
    m_channelControls[i]->setColor(m_color);
    m_channelControls[i]->update();
  }

  m_hexagonalColorWheel->setColor(m_color);
  m_hexagonalColorWheel->update();

  m_squaredColorWheel->setColor(m_color);
  m_squaredColorWheel->update();

  bool signalsBlocked = m_verticalSlider->blockSignals(true);
  m_verticalSlider->setColor(m_color);
  m_verticalSlider->setValue(m_color.getValue(m_verticalSlider->getChannel()));
  m_verticalSlider->update();
  m_verticalSlider->blockSignals(signalsBlocked);

  if (m_variationStrip)
    static_cast<ColorVariationStrip *>(m_variationStrip)->setFrom(m_color);
}

//-----------------------------------------------------------------------------

void PlainColorPage::setColor(const TColorStyle &style,
                              int colorParameterIndex) {
  TPixel32 newPixel = style.getColorParamValue(colorParameterIndex);
  if (m_color.getTPixel() == newPixel) return;
  bool oldSignalEnabled = m_signalEnabled;
  m_signalEnabled       = false;
  m_color.setTPixel(newPixel);
  updateControls();
  m_signalEnabled = oldSignalEnabled;
}

//-----------------------------------------------------------------------------

void PlainColorPage::setIsVertical(bool isVertical) {
  // if (m_isVertical == isVertical) return;
  // not returning even if it already is the same orientation
  // to take advantage of the resizing here
  // this is useful for the first time the splitter is set
  // afterwards, it will be overridden by the saved state
  // from settings.
  m_isVertical = isVertical;
  if (isVertical) {
    m_vSplitter->setOrientation(Qt::Vertical);
    QList<int> sectionSizes;
    // maximize color wheel space
    sectionSizes << height() - 1 << 1;
    m_vSplitter->setSizes(sectionSizes);
  } else {
    m_vSplitter->setOrientation(Qt::Horizontal);
    QList<int> sectionSizes;
    sectionSizes << width() / 2 << width() / 2;
    m_vSplitter->setSizes(sectionSizes);
  }
}

//-----------------------------------------------------------------------------

void PlainColorPage::toggleOrientation() { setIsVertical(!m_isVertical); }

//-----------------------------------------------------------------------------

QByteArray PlainColorPage::getSplitterState() {
  return m_vSplitter->saveState();
}

//-----------------------------------------------------------------------------

void PlainColorPage::setSplitterState(QByteArray state) {
  m_vSplitter->restoreState(state);
}

//-----------------------------------------------------------------------------

void PlainColorPage::updateColorCalibration() {
  if (m_hexagonalColorWheel->isVisible())
    m_hexagonalColorWheel->updateColorCalibration();
  else
    m_hexagonalColorWheel->cueCalibrationUpdate();
}

//-----------------------------------------------------------------------------

void PlainColorPage::setColorPageMode(ColorPageMode mode) {
  m_hexagonalColorWheel->setPageMode(mode);
  updatePickerChrome();
}

//-----------------------------------------------------------------------------

ColorPageMode PlainColorPage::colorPageMode() const {
  return m_hexagonalColorWheel->pageMode();
}

//-----------------------------------------------------------------------------

void PlainColorPage::setAdvancedSvShape(AdvancedSvShape shape) {
  m_hexagonalColorWheel->setSvShape(shape);
  updatePickerChrome();
}

//-----------------------------------------------------------------------------

AdvancedSvShape PlainColorPage::advancedSvShape() const {
  return m_hexagonalColorWheel->svShape();
}

//-----------------------------------------------------------------------------

void PlainColorPage::setPickerKind(AdvancedPickerKind kind) {
  m_pickerKind = kind;
  updatePickerChrome();
}

//-----------------------------------------------------------------------------

void PlainColorPage::setPickerVisible(bool on) {
  m_pickerVisible = on;
  updatePickerChrome();
}

//-----------------------------------------------------------------------------

void PlainColorPage::updatePickerChrome() {
  const bool advanced =
      m_hexagonalColorWheel->pageMode() == ColorPageMode::Advanced;
  const bool pickerOn = !advanced || m_pickerVisible;
  const bool rectangle =
      advanced && pickerOn && m_pickerKind == AdvancedPickerKind::Rectangle;
  const bool wheelAdv     = advanced && pickerOn && !rectangle;
  const bool showAdvBtn   = StyleEditorShowAdvancedModeButton != 0;
  const bool showShapeBtn = wheelAdv && StyleEditorShowSvShapeButton != 0;
  const bool showSections = StyleEditorShowSectionToggles != 0;
  const bool showKindBtns =
      advanced && StyleEditorShowPickerKindButtons != 0;

  m_hexagonalColorWheel->setVisible(pickerOn && !rectangle);
  m_rectPicker->setVisible(rectangle);
  if (m_pickerSectionAction)
    m_pickerFrame->setVisible(m_pickerSectionAction->isChecked() && pickerOn);

  for (int i = 0; i < 7; ++i)
    m_channelControls[i]->setModeRadioVisible(rectangle);

  m_advancedModeBtn->setVisible(showAdvBtn);
  m_advancedModeBtn->setChecked(advanced);
  m_advancedModeBtn->setToolTip(advanced ? tr("Switch to Classic")
                                         : tr("Switch to Advanced"));
  m_wheelKindBtn->setVisible(showKindBtns);
  m_rectKindBtn->setVisible(showKindBtns);
  m_wheelKindBtn->setChecked(wheelAdv);
  m_rectKindBtn->setChecked(rectangle);
  const bool showVarBtn = StyleEditorShowVarButton != 0;
  for (QToolButton *btn : m_sectionBar->findChildren<QToolButton *>()) {
    if (btn == m_varBtn)
      btn->setVisible(showVarBtn);
    else
      btn->setVisible(showSections);
  }
  m_sectionBar->setVisible(showSections || showVarBtn);
  m_svShapeBtn->setVisible(showShapeBtn);
  if (m_hexagonalColorWheel->svShape() == AdvancedSvShape::Square) {
    m_svShapeBtn->setIcon(createQIcon("colorpicker_sv_triangle"));
    m_svShapeBtn->setToolTip(tr("Switch to the triangular chromatic space"));
  } else {
    m_svShapeBtn->setIcon(createQIcon("colorpicker_sv_square"));
    m_svShapeBtn->setToolTip(tr("Switch to the square chromatic space"));
  }
  const bool showTopChrome =
      showAdvBtn || showSections || showKindBtns || showVarBtn;
  m_pickerChrome->setVisible(showTopChrome);
  if (QLayout *lay = m_pickerChrome->layout()) lay->activate();
  m_pickerChrome->updateGeometry();
  m_sectionBar->updateGeometry();
  fitPickerChrome();
  placeSvShapeButton();
  QTimer::singleShot(0, this, SLOT(refreshPickerLayout()));
}

//-----------------------------------------------------------------------------

void PlainColorPage::refreshPickerLayout() {
  if (QLayout *wl = m_pickerFrame ? m_pickerFrame->layout() : 0) {
    wl->invalidate();
    wl->activate();
  }
  if (m_rectPicker && m_rectPicker->isVisible()) {
    m_rectPicker->updateGeometry();
    static_cast<RectanglePickerPane *>(m_rectPicker)->relayout();
  }
  if (m_hexagonalColorWheel && m_hexagonalColorWheel->isVisible())
    m_hexagonalColorWheel->refreshLayout();
  placeSvShapeButton();
}

//-----------------------------------------------------------------------------

void PlainColorPage::bindSectionActions(QAction *picker, QAction *alpha,
                                        QAction *hsv, QAction *rgb,
                                        QAction *hex, QAction *swatch) {
  m_pickerSectionAction = picker;
  QHBoxLayout *lay = qobject_cast<QHBoxLayout *>(m_sectionBar->layout());
  if (!lay) return;

  auto addBtn = [&](QAction *action, const QString &label,
                    const QString &tip) -> QToolButton * {
    QToolButton *btn = new QToolButton(m_sectionBar);
    btn->setCheckable(true);
    btn->setAutoRaise(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setFixedHeight(20);
    btn->setStyleSheet(QStringLiteral(
        "QToolButton { font-size: 10px; padding: 0px 2px; margin: 0px; "
        "min-width: 0px; min-height: 0px; }"
        "QToolButton:hover, QToolButton:checked, QToolButton:checked:hover { "
        "padding: 0px 2px; margin: 0px; }"));
    btn->setText(label);
    btn->setToolTip(tip);
    btn->setChecked(action->isChecked());
    connect(btn, SIGNAL(toggled(bool)), action, SLOT(setChecked(bool)));
    connect(action, SIGNAL(toggled(bool)), btn, SLOT(setChecked(bool)));
    lay->addWidget(btn, 0, Qt::AlignVCenter);
    return btn;
  };

  addBtn(picker, tr("CP"), tr("Color picker"));
  addBtn(alpha, tr("A"), tr("Alpha slider"));
  addBtn(hsv, tr("HSV"), tr("HSV sliders"));
  addBtn(rgb, tr("RGB"), tr("RGB sliders"));
  addBtn(hex, tr("HEX"), tr("Hex"));
  m_varBtn = addBtn(swatch, tr("VAR"), tr("Color variations"));
  updatePickerChrome();
}

//-----------------------------------------------------------------------------

bool PlainColorPage::connectPickerContextMenu(const QObject *receiver,
                                             const char *member) {
  bool ok = connect(m_hexagonalColorWheel, SIGNAL(contextMenuRequested(QPoint)),
                    receiver, member);
  ok = connect(this, SIGNAL(pickerContextMenuRequested(QPoint)), receiver,
               member) &&
       ok;
  return ok;
}

//-----------------------------------------------------------------------------

void PlainColorPage::onRectPickerContextMenu(const QPoint &pos) {
  emit pickerContextMenuRequested(m_rectPicker->mapToGlobal(pos));
}

//-----------------------------------------------------------------------------

void PlainColorPage::onPageContextMenu(const QPoint &pos) {
  QWidget *w = qobject_cast<QWidget *>(sender());
  emit pickerContextMenuRequested(w ? w->mapToGlobal(pos) : QCursor::pos());
}

//-----------------------------------------------------------------------------

void PlainColorPage::contextMenuEvent(QContextMenuEvent *event) {
  emit pickerContextMenuRequested(event->globalPos());
  event->accept();
}

//-----------------------------------------------------------------------------

void PlainColorPage::onPickerKindButtonClicked() {
  QToolButton *btn = qobject_cast<QToolButton *>(sender());
  if (!btn) return;
  if (!btn->isChecked()) {
    emit pickerKindClicked(-1);
    return;
  }
  emit pickerKindClicked(btn->property("kind").toInt());
}

//-----------------------------------------------------------------------------

void PlainColorPage::setWheelChannel(int channel) {
  assert(0 <= channel && channel < 7);
  m_squaredColorWheel->setChannel((ColorChannel)channel);
  bool signalsBlocked = m_verticalSlider->blockSignals(true);
  m_verticalSlider->setChannel((ColorChannel)channel);
  m_verticalSlider->setRange(0, ChannelMaxValues[channel]);
  m_verticalSlider->setValue(m_color.getValue((ColorChannel)channel));
  m_verticalSlider->update();
  m_verticalSlider->blockSignals(signalsBlocked);
  m_squaredColorWheel->update();
}

//-----------------------------------------------------------------------------

void PlainColorPage::onWheelSliderChanged(int value) {
  if (m_color.getValue(m_verticalSlider->getChannel()) == value) return;
  m_color.setValue(m_verticalSlider->getChannel(), value);
  updateControls();
  if (m_signalEnabled) emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

void PlainColorPage::onWheelSliderReleased() {
  if (m_signalEnabled) emit colorChanged(m_color, false);
}

//-----------------------------------------------------------------------------

void PlainColorPage::onControlChanged(const ColorModel &color,
                                      bool isDragging) {
  if (!(m_color == color)) {
    m_color = color;
    updateControls();
  }

  if (m_signalEnabled) emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------

void PlainColorPage::onWheelChanged(const ColorModel &color, bool isDragging) {
  if (!(m_color == color)) {
    m_color = color;
    updateControls();
  }
  if (m_signalEnabled) emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------

//*****************************************************************************
//    StyleChooserPage  implementation
//*****************************************************************************

TFilePath StyleChooserPage::m_rootPath;

//-----------------------------------------------------------------------------

StyleChooserPage::StyleChooserPage(StyleEditor *styleEditor, QWidget *parent)
    : StyleEditorPage(parent)
    , m_chipOrigin(5, 3)
    , m_chipSize(25, 25)
    , m_chipPerRow(0)
    , m_pinsToTopDirty(false)
    , m_styleEditor(styleEditor) {
  //, m_currentIndex(-1) {

  setObjectName("StyleChooserPage");

  m_pinToTopAct = new QAction(tr("Pin To Top"), this);
  m_pinToTopAct->setCheckable(true);
  m_setPinsToTopAct = new QAction(tr("Set Pins To Top"), this);
  m_clrPinsToTopAct = new QAction(tr("Clear Pins To Top"), this);

  FavoritesManager *favorites = FavoritesManager::instance();

  bool ret = true;

  ret = ret && connect(m_pinToTopAct, SIGNAL(triggered()), this,
                       SLOT(togglePinToTop()));
  ret = ret && connect(m_setPinsToTopAct, SIGNAL(triggered()), this,
                       SLOT(doSetPinsToTop()));
  ret = ret && connect(m_clrPinsToTopAct, SIGNAL(triggered()), this,
                       SLOT(doClrPinsToTop()));
  ret = ret && connect(favorites, SIGNAL(pinsToTopChanged()), this,
                       SLOT(doPinsToTopChange()));
  assert(ret);
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------

void StyleChooserPage::setChipSize(QSize chipSize) {
  if (chipSize.width() < 4) chipSize.setWidth(4);
  if (chipSize.height() < 4) chipSize.setHeight(4);
  m_chipSize = chipSize;
  computeSize();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::applyFilter() {
  assert(m_manager);
  m_manager->applyFilter();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::applyFilter(const QString text) {
  assert(m_manager);
  m_manager->applyFilter(text);
}

//-----------------------------------------------------------------------------

void StyleChooserPage::paintEvent(QPaintEvent *) {
  if (loadIfNeeded() || m_pinsToTopDirty) {
    m_pinsToTopDirty = false;
    applyFilter();
    computeSize();
  }

  // Get current selected style
  TColorStyleP selectedStyle = nullptr;
  if (m_styleEditor) selectedStyle = m_styleEditor->getEditedStyle();

  QPainter p(this);
  // p.setRenderHint(QPainter::SmoothPixmapTransform);

  int maxCount = getChipCount();
  if (m_chipPerRow == 0 || maxCount == 0) return;

  int w      = parentWidget()->width();
  int chipLx = m_chipSize.width(), chipLy = m_chipSize.height();
  int nX = m_chipPerRow;
  int nY = (maxCount + m_chipPerRow - 1) / m_chipPerRow;
  int x0 = m_chipOrigin.x();
  int y0 = m_chipOrigin.y();
  int i, j;
  QRect currentIndexRect = QRect();
  int count              = 0;
  for (i = 0; i < nY; i++)
    for (j = 0; j < nX; j++) {
      QRect rect(x0 + chipLx * j + 2, y0 + chipLy * i + 2, chipLx, chipLy);

      int chipType = drawChip(p, rect, count);
      if (chipType == COMMONCHIP) {
        p.setPen(m_commonChipBoxColor);
        p.drawRect(rect);
      } else if (chipType == PINNEDCHIP) {
        p.setPen(m_pinnedChipBoxColor);
        p.drawRect(rect.adjusted(0, 0, -1, -1));
      } else {  // SOLIDCHIP
        p.setPen(m_solidChipBoxColor);
        p.drawRect(rect.adjusted(0, 0, -1, -1));
      }

      // if (m_currentIndex == count) currentIndexRect = rect;
      if (isSameStyle(selectedStyle, count)) currentIndexRect = rect;

      count++;
      if (count >= maxCount) break;
    }

  if (!currentIndexRect.isEmpty()) {
    // Draw the curentIndex border
    p.setPen(m_selectedChipBoxColor);
    p.drawRect(currentIndexRect);
    p.setPen(m_selectedChipBox2Color);
    p.drawRect(currentIndexRect.adjusted(1, 1, -1, -1));
    p.setPen(m_selectedChipBoxColor);
    p.drawRect(currentIndexRect.adjusted(2, 2, -2, -2));
    p.setPen(m_commonChipBoxColor);
    p.drawRect(currentIndexRect.adjusted(3, 3, -3, -3));
  }
}

//-----------------------------------------------------------------------------

void StyleChooserPage::patternAdded() {
  applyFilter();
  computeSize();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::computeSize() {
  int w        = width();
  m_chipPerRow = (w - 15) / m_chipSize.width();
  int rowCount = 0;
  if (m_chipPerRow != 0)
    rowCount = (getChipCount() + m_chipPerRow - 1) / m_chipPerRow;
  setMinimumSize(3 * m_chipSize.width(), rowCount * m_chipSize.height() + 10);
  update();
}

//-----------------------------------------------------------------------------

int StyleChooserPage::posToIndex(const QPoint &pos) const {
  if (m_chipPerRow == 0) return -1;

  int x = (pos.x() - m_chipOrigin.x() - 2) / m_chipSize.width();
  if (x >= m_chipPerRow) return -1;

  int y = (pos.y() - m_chipOrigin.y() - 2) / m_chipSize.height();

  int index = x + m_chipPerRow * y;
  if (index < 0 || index >= getChipCount()) return -1;

  return index;
}

//-----------------------------------------------------------------------------

void StyleChooserPage::mousePressEvent(QMouseEvent *event) {
  QPoint pos       = event->pos();
  int currentIndex = posToIndex(pos);
  if (currentIndex < 0) return;
  // m_currentIndex = currentIndex;
  onSelect(currentIndex);

  update();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::mouseMoveEvent(QMouseEvent *event) {
  QPoint pos       = event->pos();
  int currentIndex = posToIndex(pos);
  if (currentIndex >= 0 && currentIndex < getChipCount())
    setCursor(Qt::PointingHandCursor);
  else
    setCursor(Qt::ArrowCursor);
}

//-----------------------------------------------------------------------------

void StyleChooserPage::mouseReleaseEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

void StyleChooserPage::contextMenuEvent(QContextMenuEvent *event) {
  QPoint pos       = event->pos();
  int currentIndex = posToIndex(pos);
  if (currentIndex < 0) return;

  // Get current selected style
  TColorStyleP selectedStyle = nullptr;
  if (m_styleEditor) selectedStyle = m_styleEditor->getEditedStyle();

  if (!selectedStyle) return;
  std::string idname = selectedStyle->getBrushIdName();

  // Blacklist "no brush" since it's always pinned/favorite
  if (idname == "SolidColorStyle") return;

  QMenu menu(this);

  FavoritesManager *favorites = FavoritesManager::instance();

  m_pinToTopAct->setChecked(favorites->getPinToTop(idname));
  menu.addAction(m_pinToTopAct);
  // menu.addSeparator();
  // QMenu *menuvis = menu.addMenu("Visible Brushes");
  // menuvis->addAction(m_setPinsToTopAct);
  // menuvis->addAction(m_clrPinsToTopAct);
  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

bool StyleChooserPage::event(QEvent *e) {
  // Intercept tooltip events
  if (e->type() != QEvent::ToolTip) return StyleEditorPage::event(e);

  // see StyleChooserPage::paintEvent
  QHelpEvent *he = static_cast<QHelpEvent *>(e);

  int chipIdx = posToIndex(he->pos()), chipCount = getChipCount();
  if (chipIdx < 0 || chipIdx >= chipCount) {
    QToolTip::hideText();
    return false;
  }

  QString toolTip = getChipDescription(chipIdx);
  if (toolTip.isEmpty())
    QToolTip::hideText();
  else
    QToolTip::showText(he->globalPos(), toolTip);

  return true;
}

//-----------------------------------------------------------------------------

void StyleChooserPage::togglePinToTop() {
  // Get current selected style
  TColorStyleP selectedStyle = nullptr;
  if (m_styleEditor) selectedStyle = m_styleEditor->getEditedStyle();

  if (!selectedStyle) return;
  std::string idname = selectedStyle->getBrushIdName();

  FavoritesManager *favorites = FavoritesManager::instance();

  favorites->togglePinToTop(idname);
  favorites->savePinsToTop();
  favorites->emitPinsToTopChange();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::doSetPinsToTop() {
  FavoritesManager *favorites = FavoritesManager::instance();

  int len = m_manager->countData();
  for (int i = 0; i < len; i++) {
    auto &data = m_manager->getData(i);
    favorites->setPinToTop(data.idname, true);
  }
  favorites->savePinsToTop();
  favorites->emitPinsToTopChange();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::doClrPinsToTop() {
  FavoritesManager *favorites = FavoritesManager::instance();

  int len = m_manager->countData();
  for (int i = 0; i < len; i++) {
    auto &data = m_manager->getData(i);
    favorites->setPinToTop(data.idname, false);
  }
  favorites->savePinsToTop();
  favorites->emitPinsToTopChange();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::doPinsToTopChange() {
  if (!m_pinsToTopDirty) m_pinsToTopDirty = true;
  update();
}

//-----------------------------------------------------------------------------
// Remove
void StyleChooserPage::setRootPath(const TFilePath &rootPath) {
  m_rootPath = rootPath;
}

//*****************************************************************************
//    CustomStyleChooser  implementation
//*****************************************************************************

int CustomStyleChooserPage::drawChip(QPainter &p, QRect rect, int index) {
  assert(0 <= index && index < getChipCount());
  auto &data = m_manager->getData(index);
  if (!data.image.isNull()) p.drawImage(rect, data.image);
  return data.markPinToTop ? PINNEDCHIP : COMMONCHIP;
}

//-----------------------------------------------------------------------------

void CustomStyleChooserPage::onSelect(int index) {
  if (index < 0 || index >= getChipCount()) return;

  auto &data = m_manager->getData(index);

  std::string name = data.name.toStdString();

  if (data.isVector) {
    TVectorImagePatternStrokeStyle cs(name);
    emit styleSelected(cs);
  } else {
    TRasterImagePatternStrokeStyle cs(name);
    emit styleSelected(cs);
  }
}

//-----------------------------------------------------------------------------

bool CustomStyleChooserPage::isSameStyle(const TColorStyleP style, int index) {
  return style->getBrushIdHash() == m_manager->getData(index).hash;
}

//-----------------------------------------------------------------------------

QString CustomStyleChooserPage::getChipDescription(int index) {
  return m_manager->getData(index).desc;
}

//*****************************************************************************
//    VectorBrushStyleChooser  implementation
//*****************************************************************************

int VectorBrushStyleChooserPage::drawChip(QPainter &p, QRect rect, int index) {
  if (index == 0) {
    static QImage noSpecialStyleImage(":Resources/no_vectorbrush.png");
    p.drawImage(rect, noSpecialStyleImage);
    return SOLIDCHIP;
  } else {
    auto &data = m_manager->getData(index - 1);
    p.drawImage(rect, data.image);
    return data.markPinToTop ? PINNEDCHIP : COMMONCHIP;
  }
}

//-----------------------------------------------------------------------------

void VectorBrushStyleChooserPage::onSelect(int index) {
  if (index < 0 || index >= getChipCount()) return;

  if (index > 0) {
    auto &data = m_manager->getData(index - 1);

    std::string name = data.name.toStdString();
    assert(data.isVector);  // must be Vector
    if (!data.isVector) return;

    TVectorBrushStyle cs(name);
    emit styleSelected(cs);
  } else {
    static TSolidColorStyle noStyle(TPixel32::Black);
    emit styleSelected(noStyle);
  }
}

//-----------------------------------------------------------------------------

bool VectorBrushStyleChooserPage::isSameStyle(const TColorStyleP style,
                                              int index) {
  if (index > 0) {
    auto &data = m_manager->getData(index - 1);
    if (!data.isVector) return false;  // must be Vector
    return style->getBrushIdHash() == data.hash;
  } else
    return style->getBrushIdHash() == TSolidColorStyle::staticBrushIdHash();
}

//-----------------------------------------------------------------------------

QString VectorBrushStyleChooserPage::getChipDescription(int index) {
  if (index > 0)
    return m_manager->getData(index - 1).desc;
  else
    return QObject::tr("Plain color", "VectorBrushStyleChooserPage");
}

//*****************************************************************************
//    TextureStyleChooser  implementation
//*****************************************************************************

int TextureStyleChooserPage::drawChip(QPainter &p, QRect rect, int index) {
  assert(0 <= index && index < getChipCount());

  if (index == 0) {
    static QImage noStyleImage(":Resources/no_texturestyle.png");
    p.drawImage(rect, noStyleImage);
    return SOLIDCHIP;
  } else {
    auto &data = m_manager->getData(index - 1);
    p.drawImage(rect, data.image);
    return data.markPinToTop ? PINNEDCHIP : COMMONCHIP;
  }
}

//-----------------------------------------------------------------------------

void TextureStyleChooserPage::onSelect(int index) {
  assert(0 <= index && index < getChipCount());

  if (index == 0) {
    static TSolidColorStyle noStyle(TPixel32::Black);
    emit styleSelected(noStyle);
  } else {
    auto &data = m_manager->getData(index - 1);

    TTextureStyle style(data.raster, TFilePath(data.name.toStdWString()));
    emit styleSelected(style);
  }
}

//-----------------------------------------------------------------------------

bool TextureStyleChooserPage::isSameStyle(const TColorStyleP style, int index) {
  if (index > 0)
    return style->getBrushIdHash() == m_manager->getData(index - 1).hash;
  else
    return style->getBrushIdHash() == TSolidColorStyle::staticBrushIdHash();
}

//-----------------------------------------------------------------------------

QString TextureStyleChooserPage::getChipDescription(int index) {
  if (index > 0)
    return m_manager->getData(index - 1).desc;
  else
    return QObject::tr("Plain color", "TextureStyleChooserPage");
}

//*****************************************************************************
//    MyPaintBrushStyleChooserPage  implementation
//*****************************************************************************

int MyPaintBrushStyleChooserPage::drawChip(QPainter &p, QRect rect, int index) {
  assert(0 <= index && index < getChipCount());
  if (index == 0) {
    static QImage noStyleImage(":Resources/no_mypaintbrush.png");
    p.drawImage(rect, noStyleImage);
    return SOLIDCHIP;
  } else {
    auto &data = m_manager->getData(index - 1);
    p.drawImage(rect, data.image);
    return data.markPinToTop ? PINNEDCHIP : COMMONCHIP;
  }
}

//-----------------------------------------------------------------------------

void MyPaintBrushStyleChooserPage::onSelect(int index) {
  assert(0 <= index && index < getChipCount());
  if (index == 0) {
    static TSolidColorStyle noStyle(TPixel32::Black);
    emit styleSelected(noStyle);
  } else {
    --index;
    emit styleSelected(getBrush(index));
  }
}

//-----------------------------------------------------------------------------

bool MyPaintBrushStyleChooserPage::isSameStyle(const TColorStyleP style,
                                               int index) {
  if (index > 0)
    return style->getBrushIdHash() == getBrush(index - 1).getBrushIdHash();
  else
    return style->getBrushIdHash() == TSolidColorStyle::staticBrushIdHash();
}

//-----------------------------------------------------------------------------

QString MyPaintBrushStyleChooserPage::getChipDescription(int index) {
  if (index > 0)
    return m_manager->getData(index - 1).desc;
  else
    return QObject::tr("Plain color", "MyPaintBrushStyleChooserPage");
}

//*****************************************************************************
//    SpecialStyleChooser  implementation
//*****************************************************************************

int SpecialStyleChooserPage::drawChip(QPainter &p, QRect rect, int index) {
  if (index == 0) {
    static QImage noSpecialStyleImage(":Resources/no_specialstyle.png");
    p.drawImage(rect, noSpecialStyleImage);
    return SOLIDCHIP;
  } else {
    auto &data = m_manager->getData(index - 1);
    p.drawImage(rect, data.image);
    return data.markPinToTop ? PINNEDCHIP : COMMONCHIP;
  }
}

//-----------------------------------------------------------------------------

void SpecialStyleChooserPage::onSelect(int index) {
  assert(0 <= index && index < getChipCount());
  TColorStyle *cs = 0;
  // if (m_currentIndex < 0) return;
  if (index == 0)
    cs = new TSolidColorStyle(TPixel32::Black);
  else {
    auto &data = m_manager->getData(index - 1);

    cs = TColorStyle::create(data.tagId);
  }

  emit styleSelected(*cs);
}

//-----------------------------------------------------------------------------

bool SpecialStyleChooserPage::isSameStyle(const TColorStyleP style, int index) {
  if (index > 0)
    return style->getBrushIdHash() == m_manager->getData(index - 1).hash;
  else
    return style->getBrushIdHash() == TSolidColorStyle::staticBrushIdHash();
}

//-----------------------------------------------------------------------------

QString SpecialStyleChooserPage::getChipDescription(int index) {
  if (index > 0)
    return m_manager->getData(index - 1).desc;
  else
    return QObject::tr("Plain color", "SpecialStyleChooserPage");
}

//=============================================================================
// SettingBox
//-----------------------------------------------------------------------------
/*
SettingBox::SettingBox(QWidget *parent, int index)
: QWidget(parent)
, m_index(index)
, m_style(0)
{
        QHBoxLayout* hLayout = new QHBoxLayout(this);
        hLayout->setSpacing(5);
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->addSpacing(10);
        m_name = new QLabel(this);
        m_name->setFixedSize(82,20);
        m_name->setStyleSheet("border: 0px;");
        hLayout->addWidget(m_name,0);
        m_doubleField = new DoubleField(this, true);
        hLayout->addWidget(m_doubleField,1);
        hLayout->addSpacing(10);
        bool ret = connect(m_doubleField, SIGNAL(valueChanged(bool)), this,
SLOT(onValueChanged(bool)));
  assert(ret);
        setLayout(hLayout);
        setFixedHeight(22);
}

//-----------------------------------------------------------------------------

void SettingBox::setParameters(TColorStyle* cs)
{
        if(!cs)
        {
                m_style = cs;
                return;
        }
        if(cs->getParamCount() == 0 || m_index<0 ||
cs->getParamCount()<=m_index)
                return;
        QString paramName = cs->getParamNames(m_index);
        m_name->setText(paramName);
   double newValue = cs->getParamValue(TColorStyle::double_tag(), m_index);
        double value = m_doubleField->getValue();
        m_style = cs;
        if(value != newValue)
        {
                double min=0, max=1;
                cs->getParamRange(m_index,min,max);
                m_doubleField->setValues(newValue, min, max);
        }
        TCleanupStyle* cleanupStyle = dynamic_cast<TCleanupStyle*>(cs);
        if(paramName == "Contrast" && cleanupStyle)
        {
                if(!cleanupStyle->isContrastEnabled())
                        m_doubleField->setEnabled(false);
                else
                        m_doubleField->setEnabled(true);
        }
        update();
}

//-----------------------------------------------------------------------------

void SettingBox::setColorStyleParam(double value, bool isDragging)
{
  TColorStyle* style = m_style;
  assert(style && m_index < style->getParamCount());

  double min = 0.0, max = 1.0;
  style->getParamRange(m_index, min, max);

  style->setParamValue(m_index, tcrop(value, min, max));

  emit valueChanged(*style, isDragging);
}

//-----------------------------------------------------------------------------

void SettingBox::onValueChanged(bool isDragging)
{
        if(!m_style || m_style->getParamCount() == 0)
                return;

        double value = m_doubleField->getValue();
        if(isDragging && m_style->getParamValue(TColorStyle::double_tag(),
m_index) == value)
                return;

        setColorStyleParam(value, isDragging);
}
*/

//*****************************************************************************
//    SettingsPage  implementation
//*****************************************************************************

SettingsPage::SettingsPage(QWidget *parent)
    : QScrollArea(parent), m_updating(false) {
  bool ret = true;

  setObjectName("styleEditorPage");  // It is necessary for the styleSheet
  setFrameStyle(QFrame::StyledPanel);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWidgetResizable(true);

  // Build the scrolled widget
  QFrame *paramsContainer = new QFrame(this);
  setWidget(paramsContainer);

  QVBoxLayout *paramsContainerLayout = new QVBoxLayout(this);
  paramsContainerLayout->setContentsMargins(10, 10, 10, 10);
  paramsContainerLayout->setSpacing(10);
  paramsContainer->setLayout(paramsContainerLayout);

  // Add a vertical layout to store the "autofill" checkbox widgets
  m_autoFillCheckBox = new QCheckBox(tr("Autopaint for Lines"), this);
  paramsContainerLayout->addWidget(m_autoFillCheckBox, 0,
                                   Qt::AlignLeft | Qt::AlignVCenter);

  ret = connect(m_autoFillCheckBox, SIGNAL(stateChanged(int)), this,
                SLOT(onAutofillChanged()));
  assert(ret);

  // Prepare the style parameters layout
  m_paramsLayout = new QGridLayout;
  m_paramsLayout->setContentsMargins(0, 0, 0, 0);
  m_paramsLayout->setVerticalSpacing(8);
  m_paramsLayout->setHorizontalSpacing(5);
  paramsContainerLayout->addLayout(m_paramsLayout);

  paramsContainerLayout->addStretch(1);
}

//-----------------------------------------------------------------------------

void SettingsPage::enableAutopaintToggle(bool enabled) {
  m_autoFillCheckBox->setVisible(enabled);
}

//-----------------------------------------------------------------------------

void SettingsPage::setStyle(const TColorStyleP &editedStyle) {
  struct locals {
    inline static void clearLayout(QLayout *layout) {
      QLayoutItem *item;
      while ((item = layout->takeAt(0)) != 0) {
        delete item->layout();
        delete item->spacerItem();
        delete item->widget();
        delete item;
      }
    }
  };  // locals

  // NOTE: Layout reubilds must be avoided whenever possible. In particular, be
  // warned that this
  // function may be invoked when signals emitted from this function are still
  // "flying"...

  bool clearLayout =
      m_editedStyle &&
      !(editedStyle && typeid(*m_editedStyle) == typeid(*editedStyle));
  bool buildLayout =
      editedStyle &&
      !(m_editedStyle && typeid(*m_editedStyle) == typeid(*editedStyle));

  m_editedStyle = editedStyle;

  if (clearLayout) locals::clearLayout(m_paramsLayout);

  if (buildLayout) {
    assert(m_paramsLayout->count() == 0);

    // Assign new settings widgets - one label/editor for each parameter
    bool ret = true;

    int p, pCount = editedStyle->getParamCount();
    for (p = 0; p != pCount; ++p) {
      // Assign label
      QLabel *label = new QLabel(editedStyle->getParamNames(p));
      m_paramsLayout->addWidget(label, p, 0);

      // Assign parameter
      switch (editedStyle->getParamType(p)) {
      case TColorStyle::BOOL: {
        QCheckBox *checkBox = new QCheckBox;
        m_paramsLayout->addWidget(checkBox, p, 1);

        ret = QObject::connect(checkBox, SIGNAL(toggled(bool)), this,
                               SLOT(onValueChanged())) &&
              ret;

        break;
      }

      case TColorStyle::INT: {
        DVGui::IntField *intField = new DVGui::IntField;
        m_paramsLayout->addWidget(intField, p, 1);

        int min, max;
        m_editedStyle->getParamRange(p, min, max);

        intField->setRange(min, max);

        ret = QObject::connect(intField, SIGNAL(valueChanged(bool)), this,
                               SLOT(onValueChanged(bool))) &&
              ret;

        break;
      }

      case TColorStyle::ENUM: {
        QComboBox *comboBox = new QComboBox;
        m_paramsLayout->addWidget(comboBox, p, 1);

        QStringList items;
        m_editedStyle->getParamRange(p, items);

        comboBox->addItems(items);

        ret = QObject::connect(comboBox, SIGNAL(currentIndexChanged(int)), this,
                               SLOT(onValueChanged())) &&
              ret;

        break;
      }

      case TColorStyle::DOUBLE: {
        DVGui::DoubleField *doubleField = new DVGui::DoubleField;
        m_paramsLayout->addWidget(doubleField, p, 1);

        double min, max;
        m_editedStyle->getParamRange(p, min, max);

        doubleField->setRange(min, max);

        ret = QObject::connect(doubleField, SIGNAL(valueChanged(bool)), this,
                               SLOT(onValueChanged(bool))) &&
              ret;

        break;
      }

      case TColorStyle::FILEPATH: {
        DVGui::FileField *fileField = new DVGui::FileField;
        m_paramsLayout->addWidget(fileField, p, 1);

        QStringList extensions;
        m_editedStyle->getParamRange(p, extensions);

        fileField->setFileMode(QFileDialog::AnyFile);
        fileField->setFilters(extensions);

        fileField->setPath(QString::fromStdWString(
            editedStyle->getParamValue(TColorStyle::TFilePath_tag(), p)
                .getWideString()));

        ret = QObject::connect(fileField, SIGNAL(pathChanged()), this,
                               SLOT(onValueChanged())) &&
              ret;

        break;
      }
      }

      // "reset to default" button
      if (m_editedStyle->hasParamDefault(p)) {
        QPushButton *pushButton = new QPushButton;
        pushButton->setToolTip(tr("Reset to default"));
        pushButton->setIcon(createQIcon("delete"));
        pushButton->setFixedSize(24, 24);
        m_paramsLayout->addWidget(pushButton, p, 2);
        ret = QObject::connect(pushButton, SIGNAL(clicked(bool)), this,
                               SLOT(onValueReset())) &&
              ret;
      }

      assert(ret);
    }
  }

  updateValues();
}

//-----------------------------------------------------------------------------

void SettingsPage::updateValues() {
  if (!m_editedStyle) return;

  struct Updating {
    SettingsPage *m_this;  // Prevent 'param changed' signals from being
    ~Updating() {
      m_this->m_updating = false;
    }  // sent when updating editor widgets - this is
  } updating = {(m_updating = true, this)};  // just a view REFRESH function.

  // Deal with the autofill
  m_autoFillCheckBox->setChecked(m_editedStyle->getFlags() & 1);

  int p, pCount = m_editedStyle->getParamCount();
  for (p = 0; p != pCount; ++p) {
    // Update state of "reset to default" button
    if (m_editedStyle->hasParamDefault(p)) {
      QPushButton *pushButton = static_cast<QPushButton *>(
          m_paramsLayout->itemAtPosition(p, 2)->widget());
      pushButton->setEnabled(m_editedStyle->isParamDefault(p));
    }

    // Update editor values
    switch (m_editedStyle->getParamType(p)) {
    case TColorStyle::BOOL: {
      QCheckBox *checkBox = static_cast<QCheckBox *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      checkBox->setChecked(
          m_editedStyle->getParamValue(TColorStyle::bool_tag(), p));

      break;
    }

    case TColorStyle::INT: {
      DVGui::IntField *intField = static_cast<DVGui::IntField *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      intField->setValue(
          m_editedStyle->getParamValue(TColorStyle::int_tag(), p));

      break;
    }

    case TColorStyle::ENUM: {
      QComboBox *comboBox = static_cast<QComboBox *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      comboBox->setCurrentIndex(
          m_editedStyle->getParamValue(TColorStyle::int_tag(), p));

      break;
    }

    case TColorStyle::DOUBLE: {
      DVGui::DoubleField *doubleField = static_cast<DVGui::DoubleField *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      doubleField->setValue(
          m_editedStyle->getParamValue(TColorStyle::double_tag(), p));

      break;
    }

    case TColorStyle::FILEPATH: {
      DVGui::FileField *fileField = static_cast<DVGui::FileField *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      fileField->setPath(QString::fromStdWString(
          m_editedStyle->getParamValue(TColorStyle::TFilePath_tag(), p)
              .getWideString()));

      break;
    }
    }
  }
}

//-----------------------------------------------------------------------------

void SettingsPage::onAutofillChanged() {
  m_editedStyle->setFlags((unsigned int)(m_autoFillCheckBox->isChecked()));

  if (!m_updating)
    emit paramStyleChanged(false);  // Forward the signal to the style editor
}

//-----------------------------------------------------------------------------

int SettingsPage::getParamIndex(const QWidget *widget) {
  int p, pCount = m_paramsLayout->rowCount();
  for (p = 0; p != pCount; ++p)
    for (int c = 0; c < 3; ++c)
      if (QLayoutItem *item = m_paramsLayout->itemAtPosition(p, c))
        if (item->widget() == widget) return p;
  return -1;
}

//-----------------------------------------------------------------------------

void SettingsPage::onValueReset() {
  assert(m_editedStyle);

  // Extract the parameter index
  QWidget *senderWidget = static_cast<QWidget *>(sender());
  int p                 = getParamIndex(senderWidget);

  assert(0 <= p && p < m_editedStyle->getParamCount());
  m_editedStyle->setParamDefault(p);

  // Forward the signal to the style editor
  if (!m_updating) emit paramStyleChanged(false);
}

//-----------------------------------------------------------------------------

void SettingsPage::onValueChanged(bool isDragging) {
  assert(m_editedStyle);

  // Extract the parameter index
  QWidget *senderWidget = static_cast<QWidget *>(sender());
  int p                 = getParamIndex(senderWidget);

  assert(0 <= p && p < m_editedStyle->getParamCount());

  // Update the style's parameter value
  switch (m_editedStyle->getParamType(p)) {
  case TColorStyle::BOOL:
    m_editedStyle->setParamValue(
        p, static_cast<QCheckBox *>(senderWidget)->isChecked());
    break;
  case TColorStyle::INT:
    m_editedStyle->setParamValue(
        p, static_cast<DVGui::IntField *>(senderWidget)->getValue());
    break;
  case TColorStyle::ENUM:
    m_editedStyle->setParamValue(
        p, static_cast<QComboBox *>(senderWidget)->currentIndex());
    break;
  case TColorStyle::DOUBLE:
    m_editedStyle->setParamValue(
        p, static_cast<DVGui::DoubleField *>(senderWidget)->getValue());
    break;
  case TColorStyle::FILEPATH: {
    const QString &string =
        static_cast<DVGui::FileField *>(senderWidget)->getPath();
    m_editedStyle->setParamValue(p, TFilePath(string.toStdWString()));
    break;
  }
  }

  // Forward the signal to the style editor
  if (!m_updating) emit paramStyleChanged(isDragging);
}

//=============================================================================

namespace {

QScrollArea *makeChooserPage(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

//-----------------------------------------------------------------------------

QScrollArea *makeChooserPageWithoutScrollBar(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

}  // namespace

//*****************************************************************************
//    StyleEditor  implementation
//*****************************************************************************

StyleEditor::StyleEditor(PaletteController *paletteController, QWidget *parent)
    : QWidget(parent)
    , m_paletteController(paletteController)
    , m_paletteHandle(paletteController->getCurrentPalette())
    , m_cleanupPaletteHandle(paletteController->getCurrentCleanupPalette())
    , m_toolBar(0)
    , m_swatchAction(0)
    , m_enabled(false)
    , m_enabledOnlyFirstTab(false)
    , m_enabledFirstAndLastTab(false)
    , m_oldStyle(0)
    , m_parent(parent)
    , m_hexColorNamesEditor(0)
    , m_editedStyle(0) {
  setFocusPolicy(Qt::NoFocus);
  // Remove
  TFilePath libraryPath = ToonzFolder::getLibraryFolder();
  setRootPath(libraryPath);

  m_styleBar = new DVGui::TabBar(this);
  m_styleBar->setDrawBase(false);
  m_styleBar->setObjectName("StyleEditorTabBar");

  // This widget is used to set the background color of the tabBar
  // using the styleSheet.
  // It is also used to take 6px on the left before the tabBar
  // and to draw the two lines on the bottom size
  m_tabBarContainer        = new TabBarContainter(this);
  m_colorParameterSelector = new ColorParameterSelector(this);

  m_plainColorPage          = new PlainColorPage(0);
  m_textureStylePage        = new TextureStyleChooserPage(this, 0);
  m_specialStylePage        = new SpecialStyleChooserPage(this, 0);
  m_customStylePage         = new CustomStyleChooserPage(this, 0);
  m_vectorBrushesStylePage  = new VectorBrushStyleChooserPage(this, 0);
  m_mypaintBrushesStylePage = new MyPaintBrushStyleChooserPage(this, 0);
  m_settingsPage            = new SettingsPage(0);

  QWidget *emptyPage = new StyleEditorPage(0);

  // For the plainColorPage and the settingsPage
  // I create a "fake" QScrollArea (without ScrollingBar
  // in order to use the styleSheet to stylish its background
  QScrollArea *plainArea = makeChooserPageWithoutScrollBar(m_plainColorPage);
  QScrollArea *textureArea =
      makeChooserPageWithoutScrollBar(createTexturePage());
  QScrollArea *mypaintBrushesArea =
      makeChooserPageWithoutScrollBar(createMyPaintPage());
  QScrollArea *settingsArea = makeChooserPageWithoutScrollBar(m_settingsPage);
  QScrollArea *vectorOutsideArea =
      makeChooserPageWithoutScrollBar(createVectorPage());
  textureArea->setMinimumWidth(50);
  vectorOutsideArea->setMinimumWidth(50);
  mypaintBrushesArea->setMinimumWidth(50);

  m_styleChooser = new QStackedWidget(this);
  m_styleChooser->addWidget(plainArea);
  m_styleChooser->addWidget(textureArea);
  m_styleChooser->addWidget(vectorOutsideArea);
  m_styleChooser->addWidget(mypaintBrushesArea);
  m_styleChooser->addWidget(settingsArea);
  m_styleChooser->addWidget(makeChooserPageWithoutScrollBar(emptyPage));
  m_styleChooser->setFocusPolicy(Qt::NoFocus);

  QFrame *bottomWidget = createBottomWidget();
  /* ------- layout ------- */
  QGridLayout *mainLayout = new QGridLayout;
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  {
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setContentsMargins(0, 0, 0, 0);
    {
      hLayout->addSpacing(0);
      hLayout->addWidget(m_styleBar);
      hLayout->addStretch();
    }
    m_tabBarContainer->setLayout(hLayout);

    mainLayout->addWidget(m_tabBarContainer, 0, 0, 1, 2);
    mainLayout->addWidget(m_styleChooser, 1, 0, 1, 2);
    mainLayout->addWidget(bottomWidget, 2, 0, 1, 2);
    // mainLayout->addWidget(m_toolBar, 3, 0);
    // mainLayout->addWidget(displayToolbar, 3, 1);
  }
  mainLayout->setColumnStretch(0, 1);
  mainLayout->setRowStretch(1, 1);
  setLayout(mainLayout);

  /* ------- signal-slot connections ------- */

  bool ret = true;
  ret      = ret && connect(m_styleBar, SIGNAL(currentChanged(int)), this,
                            SLOT(setPage(int)));
  ret = ret && connect(m_plainColorPage, SIGNAL(colorPageModeClicked()), this,
                       SLOT(onAdvancedModeButtonClicked()));
  ret = ret && connect(m_colorParameterSelector, SIGNAL(colorParamChanged()),
                       this, SLOT(onColorParamChanged()));
  ret = ret &&
        connect(m_textureStylePage, SIGNAL(styleSelected(const TColorStyle &)),
                this, SLOT(selectStyle(const TColorStyle &)));
  ret = ret &&
        connect(m_specialStylePage, SIGNAL(styleSelected(const TColorStyle &)),
                this, SLOT(selectStyle(const TColorStyle &)));
  ret = ret &&
        connect(m_customStylePage, SIGNAL(styleSelected(const TColorStyle &)),
                this, SLOT(selectStyle(const TColorStyle &)));
  ret = ret && connect(m_vectorBrushesStylePage,
                       SIGNAL(styleSelected(const TColorStyle &)), this,
                       SLOT(selectStyle(const TColorStyle &)));
  ret = ret && connect(m_mypaintBrushesStylePage,
                       SIGNAL(styleSelected(const TColorStyle &)), this,
                       SLOT(selectStyle(const TColorStyle &)));
  ret = ret && connect(m_settingsPage, SIGNAL(paramStyleChanged(bool)), this,
                       SLOT(onParamStyleChanged(bool)));
  ret = ret && connect(m_plainColorPage,
                       SIGNAL(colorChanged(const ColorModel &, bool)), this,
                       SLOT(onColorChanged(const ColorModel &, bool)));
  ret = ret && m_plainColorPage->connectPickerContextMenu(
                       this, SLOT(onPickerContextMenu(QPoint)));
  assert(ret);
  /* ------- initial conditions ------- */
  enable(false, false, false);
  // set to the empty page
  m_styleChooser->setCurrentIndex(m_styleChooser->count() - 1);
}

//-----------------------------------------------------------------------------

StyleEditor::~StyleEditor() {}

//-----------------------------------------------------------------------------
/*
void StyleEditor::setPaletteHandle(TPaletteHandle* paletteHandle)
{
        if(m_paletteHandle != paletteHandle)
                m_paletteHandle = paletteHandle;
        onStyleSwitched();
}
*/
//-----------------------------------------------------------------------------

QFrame *StyleEditor::createBottomWidget() {
  QFrame *bottomWidget = new QFrame(this);
  m_autoButton         = new QPushButton(tr("Auto"));
  m_oldColor           = new DVGui::StyleSample(this, 42, 24);
  m_newColor           = new DVGui::StyleSample(this, 42, 24);
  m_applyButton        = new QPushButton(tr("Apply"));

  bottomWidget->setFrameStyle(QFrame::StyledPanel);
  bottomWidget->setObjectName("bottomWidget");
  bottomWidget->setContentsMargins(0, 0, 0, 0);
  bottomWidget->setMinimumHeight(60);
  m_applyButton->setToolTip(tr("Apply changes to current style"));
  m_applyButton->setDisabled(m_paletteController->isColorAutoApplyEnabled());
  m_applyButton->setFocusPolicy(Qt::NoFocus);

  m_autoButton->setCheckable(true);
  m_autoButton->setToolTip(tr("Automatically update style changes"));
  m_autoButton->setChecked(m_paletteController->isColorAutoApplyEnabled());
  m_autoButton->setFocusPolicy(Qt::NoFocus);

  m_oldColor->setToolTip(tr("Return To Previous Style"));
  m_oldColor->enableClick(true);
  m_oldColor->setEnable(false);
  m_oldColor->setSystemChessboard(true);
  m_oldColor->setCloneStyle(true);
  m_newColor->setToolTip(tr("Current Style"));
  m_newColor->enableClick(true);
  m_newColor->setEnable(false);
  m_newColor->setSystemChessboard(true);

  m_hexLineEdit = new DVGui::HexLineEdit("", this);
  m_hexLineEdit->setObjectName("HexLineEdit");
  m_hexLineEdit->setFixedWidth(75);

  m_toolBar = new QToolBar(this);
  m_toolBar->setMovable(false);
  m_toolBar->setMaximumHeight(22);
  QMenu *menu    = new QMenu();
  m_pickerAction  = new QAction(tr("Color Picker"), this);
  m_hsvAction    = new QAction(tr("HSV"), this);
  m_alphaAction  = new QAction(tr("Alpha"), this);
  m_rgbAction    = new QAction(tr("RGB"), this);
  m_hexAction    = new QAction(tr("Hex"), this);
  m_swatchAction = new QAction(tr("Variations"), this);
  m_searchAction = new QAction(tr("Search"), this);

  m_pickerAction->setCheckable(true);
  m_hsvAction->setCheckable(true);
  m_alphaAction->setCheckable(true);
  m_rgbAction->setCheckable(true);
  m_hexAction->setCheckable(true);
  m_swatchAction->setCheckable(true);
  m_searchAction->setCheckable(true);
  m_pickerAction->setToolTip(tr("Color picker"));
  m_hsvAction->setToolTip(tr("HSV sliders"));
  m_alphaAction->setToolTip(tr("Alpha slider"));
  m_rgbAction->setToolTip(tr("RGB sliders"));
  m_hexAction->setToolTip(tr("Hex"));
  m_swatchAction->setToolTip(tr("Value variations of the current color"));
  m_pickerAction->setChecked(true);
  m_hsvAction->setChecked(true);
  m_alphaAction->setChecked(true);
  m_rgbAction->setChecked(true);
  m_hexAction->setChecked(false);
  m_swatchAction->setChecked(false);
  m_searchAction->setChecked(false);
  menu->addAction(m_pickerAction);
  menu->addAction(m_hsvAction);
  menu->addAction(m_alphaAction);
  menu->addAction(m_rgbAction);
  menu->addAction(m_hexAction);
  menu->addAction(m_swatchAction);
  menu->addAction(m_searchAction);

  m_sliderAppearanceAG = new QActionGroup(this);
  QAction *relColorAct =
      new QAction(tr("Relative colored + Triangle handle"), this);
  QAction *absColorAct =
      new QAction(tr("Absolute colored + Line handle"), this);
  relColorAct->setData(RelativeColoredTriangleHandle);
  absColorAct->setData(AbsoluteColoredLineHandle);
  relColorAct->setCheckable(true);
  absColorAct->setCheckable(true);
  if (StyleEditorColorSliderAppearance == RelativeColoredTriangleHandle)
    relColorAct->setChecked(true);
  else
    absColorAct->setChecked(true);
  m_sliderAppearanceAG->addAction(relColorAct);
  m_sliderAppearanceAG->addAction(absColorAct);
  m_sliderAppearanceAG->setExclusive(true);
  menu->addSeparator();
  QMenu *appearanceSubMenu = menu->addMenu(tr("Slider Appearance"));
  appearanceSubMenu->addAction(relColorAct);
  appearanceSubMenu->addAction(absColorAct);

  m_toggleOrientationAction =
      new QAction(createQIcon("orientation_h"), tr("Toggle Orientation"), this);
  menu->addAction(m_toggleOrientationAction);

  m_hexEditorAction = new QAction(tr("Hex Color Names..."), this);
  menu->addAction(m_hexEditorAction);

  QToolButton *toolButton = new QToolButton(this);
  toolButton->setIcon(createQIcon("menu"));
  toolButton->setFixedSize(22, 22);
  toolButton->setMenu(menu);
  toolButton->setPopupMode(QToolButton::InstantPopup);
  toolButton->setToolTip(tr("Show or hide parts of the Color Page."));
  // QToolBar* displayToolbar = new QToolBar(this);
  m_toolBar->addWidget(toolButton);
  m_toolBar->setMaximumHeight(22);
  m_toolBar->setIconSize(QSize(16, 16));

  /* ------ layout ------ */
  QHBoxLayout *mainLayout = new QHBoxLayout;
  mainLayout->setContentsMargins(2, 2, 2, 2);
  mainLayout->setSpacing(0);
  {
    mainLayout->addWidget(m_autoButton);
    mainLayout->addSpacing(4);
    mainLayout->addWidget(m_applyButton);
    mainLayout->addSpacing(4);

    QVBoxLayout *colorLay = new QVBoxLayout();
    colorLay->setContentsMargins(0, 0, 0, 0);
    colorLay->setSpacing(2);
    {
      QHBoxLayout *chipLay = new QHBoxLayout();
      chipLay->setContentsMargins(0, 0, 0, 0);
      chipLay->setSpacing(0);
      {
        chipLay->addWidget(m_newColor, 1);
        chipLay->addWidget(m_oldColor, 1);
      }
      colorLay->addLayout(chipLay, 1);

      colorLay->addWidget(m_colorParameterSelector, 0);
    }
    mainLayout->addLayout(colorLay, 1);
    mainLayout->addSpacing(4);

    QVBoxLayout *hexLay = new QVBoxLayout();
    hexLay->setContentsMargins(0, 0, 0, 0);
    hexLay->setSpacing(2);
    {
      hexLay->addWidget(m_hexLineEdit);
      hexLay->addWidget(m_toolBar, 0, Qt::AlignBottom | Qt::AlignRight);
    }
    mainLayout->addLayout(hexLay, 0);
  }
  bottomWidget->setLayout(mainLayout);

  /* ------ signal-slot connections ------ */
  bool ret = true;
  ret      = ret && connect(m_applyButton, SIGNAL(clicked()), this,
                            SLOT(applyButtonClicked()));
  ret      = ret && connect(m_autoButton, SIGNAL(toggled(bool)), this,
                            SLOT(autoCheckChanged(bool)));
  ret      = ret &&
        connect(m_oldColor, SIGNAL(clicked()), this, SLOT(onOldStyleClicked()));
  ret = ret &&
        connect(m_newColor, SIGNAL(clicked()), this, SLOT(onNewStyleClicked()));
  ret = ret && connect(m_pickerAction, SIGNAL(toggled(bool)),
                       m_plainColorPage, SLOT(updatePickerChrome()));
  ret = ret && connect(m_hsvAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_hsvFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_alphaAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_alphaFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_rgbAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_rgbFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_hexAction, SIGNAL(toggled(bool)), m_hexLineEdit,
                       SLOT(setVisible(bool)));
  ret = ret && connect(m_swatchAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_swatchFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_searchAction, SIGNAL(toggled(bool)), this,
                       SLOT(onSearchVisible(bool)));
  ret = ret && connect(m_hexLineEdit, SIGNAL(editingFinished()), this,
                       SLOT(onHexChanged()));
  ret = ret && connect(m_hexEditorAction, SIGNAL(triggered()), this,
                       SLOT(onHexEditor()));
  ret = ret && connect(m_toggleOrientationAction, SIGNAL(triggered()),
                       m_plainColorPage, SLOT(toggleOrientation()));
  ret = ret && connect(m_toggleOrientationAction, SIGNAL(triggered()), this,
                       SLOT(updateOrientationButton()));
  ret = ret && connect(m_sliderAppearanceAG, SIGNAL(triggered(QAction *)), this,
                       SLOT(onSliderAppearanceSelected(QAction *)));
  ret = ret && connect(m_plainColorPage, SIGNAL(svShapeClicked()), this,
                       SLOT(onSvShapeButtonClicked()));
  ret = ret && connect(m_plainColorPage, SIGNAL(pickerKindClicked(int)), this,
                       SLOT(onPickerKindClicked(int)));
  ret = ret && connect(menu, SIGNAL(aboutToShow()), this,
                       SLOT(onPopupMenuAboutToShow()));
  assert(ret);

  m_plainColorPage->bindSectionActions(m_pickerAction, m_alphaAction,
                                       m_hsvAction, m_rgbAction, m_hexAction,
                                       m_swatchAction);

  return bottomWidget;
}

//-----------------------------------------------------------------------------

QFrame *StyleEditor::createTexturePage() {
  QFrame *outsideFrame = new QFrame();
  outsideFrame->setMinimumWidth(50);

  m_textureSearchFrame = new QFrame();
  m_textureSearchText  = new QLineEdit();
  m_textureSearchClear = new QPushButton(tr("Clear Search"));
  m_textureSearchClear->setDisabled(true);
  m_textureSearchClear->setSizePolicy(QSizePolicy::Minimum,
                                      QSizePolicy::Preferred);

  /* ------ layout ------ */
  QVBoxLayout *outsideLayout = new QVBoxLayout();
  outsideLayout->setContentsMargins(0, 0, 0, 0);
  outsideLayout->setSpacing(0);
  outsideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  {
    QVBoxLayout *insideLayout = new QVBoxLayout();
    insideLayout->setContentsMargins(0, 0, 0, 0);
    insideLayout->setSpacing(0);
    insideLayout->setSizeConstraint(QLayout::SetNoConstraint);
    { insideLayout->addWidget(m_textureStylePage); }

    QFrame *insideFrame = new QFrame();
    insideFrame->setMinimumWidth(50);
    insideFrame->setLayout(insideLayout);
    m_textureArea = makeChooserPage(insideFrame);
    m_textureArea->setMinimumWidth(50);
    outsideLayout->addWidget(m_textureArea);

    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(2, 2, 2, 2);
    searchLayout->setSpacing(0);
    searchLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      searchLayout->addWidget(m_textureSearchText);
      searchLayout->addWidget(m_textureSearchClear);
    }
    m_textureSearchFrame->setLayout(searchLayout);
    outsideLayout->addWidget(m_textureSearchFrame);
  }
  outsideFrame->setLayout(outsideLayout);

  /* ------ signal-slot connections ------ */
  bool ret = true;
  ret =
      ret && connect(m_textureSearchText, SIGNAL(textChanged(const QString &)),
                     this, SLOT(onTextureSearch(const QString &)));
  ret = ret && connect(m_textureSearchClear, SIGNAL(clicked()), this,
                       SLOT(onTextureClearSearch()));
  return outsideFrame;
}

//-----------------------------------------------------------------------------

QFrame *StyleEditor::createVectorPage() {
  QFrame *vectorOutsideFrame = new QFrame();
  vectorOutsideFrame->setMinimumWidth(50);

  QPushButton *specialButton     = new QPushButton(tr("Generated"));
  QPushButton *customButton      = new QPushButton(tr("Trail"));
  QPushButton *vectorBrushButton = new QPushButton(tr("Vector Brush"));

  m_vectorsSearchFrame = new QFrame();
  m_vectorsSearchText  = new QLineEdit();
  m_vectorsSearchClear = new QPushButton(tr("Clear Search"));
  m_vectorsSearchClear->setDisabled(true);
  m_vectorsSearchClear->setSizePolicy(QSizePolicy::Minimum,
                                      QSizePolicy::Preferred);

  specialButton->setCheckable(true);
  customButton->setCheckable(true);
  vectorBrushButton->setCheckable(true);
  specialButton->setChecked(true);
  customButton->setChecked(true);
  vectorBrushButton->setChecked(true);

  /* ------ layout ------ */
  QVBoxLayout *vectorOutsideLayout = new QVBoxLayout();
  vectorOutsideLayout->setContentsMargins(0, 0, 0, 0);
  vectorOutsideLayout->setSpacing(0);
  vectorOutsideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  {
    QHBoxLayout *vectorButtonLayout = new QHBoxLayout();
    vectorButtonLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      vectorButtonLayout->addWidget(specialButton);
      vectorButtonLayout->addWidget(customButton);
      vectorButtonLayout->addWidget(vectorBrushButton);
    }
    vectorOutsideLayout->addLayout(vectorButtonLayout);

    QVBoxLayout *vectorLayout = new QVBoxLayout();
    vectorLayout->setContentsMargins(0, 0, 0, 0);
    vectorLayout->setSpacing(0);
    vectorLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      vectorLayout->addWidget(m_specialStylePage);
      vectorLayout->addWidget(m_customStylePage);
      vectorLayout->addWidget(m_vectorBrushesStylePage);
    }
    QFrame *vectorFrame = new QFrame();
    vectorFrame->setMinimumWidth(50);
    vectorFrame->setLayout(vectorLayout);
    m_vectorsArea = makeChooserPage(vectorFrame);
    m_vectorsArea->setMinimumWidth(50);
    vectorOutsideLayout->addWidget(m_vectorsArea);

    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(2, 2, 2, 2);
    searchLayout->setSpacing(0);
    searchLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      searchLayout->addWidget(m_vectorsSearchText);
      searchLayout->addWidget(m_vectorsSearchClear);
    }
    m_vectorsSearchFrame->setLayout(searchLayout);
    vectorOutsideLayout->addWidget(m_vectorsSearchFrame);
  }
  vectorOutsideFrame->setLayout(vectorOutsideLayout);

  /* ------ signal-slot connections ------ */
  bool ret = true;
  ret      = ret && connect(specialButton, SIGNAL(toggled(bool)), this,
                            SLOT(onSpecialButtonToggled(bool)));
  ret      = ret && connect(customButton, SIGNAL(toggled(bool)), this,
                            SLOT(onCustomButtonToggled(bool)));
  ret      = ret && connect(vectorBrushButton, SIGNAL(toggled(bool)), this,
                            SLOT(onVectorBrushButtonToggled(bool)));
  ret =
      ret && connect(m_vectorsSearchText, SIGNAL(textChanged(const QString &)),
                     this, SLOT(onVectorsSearch(const QString &)));
  ret = ret && connect(m_vectorsSearchClear, SIGNAL(clicked()), this,
                       SLOT(onVectorsClearSearch()));

  assert(ret);
  return vectorOutsideFrame;
}

//-----------------------------------------------------------------------------

QFrame *StyleEditor::createMyPaintPage() {
  QFrame *outsideFrame = new QFrame();
  outsideFrame->setMinimumWidth(50);

  m_mypaintSearchFrame = new QFrame();
  m_mypaintSearchText  = new QLineEdit();
  m_mypaintSearchClear = new QPushButton(tr("Clear Search"));
  m_mypaintSearchClear->setDisabled(true);
  m_mypaintSearchClear->setSizePolicy(QSizePolicy::Minimum,
                                      QSizePolicy::Preferred);

  /* ------ layout ------ */
  QVBoxLayout *outsideLayout = new QVBoxLayout();
  outsideLayout->setContentsMargins(0, 0, 0, 0);
  outsideLayout->setSpacing(0);
  outsideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  {
    QVBoxLayout *insideLayout = new QVBoxLayout();
    insideLayout->setContentsMargins(0, 0, 0, 0);
    insideLayout->setSpacing(0);
    insideLayout->setSizeConstraint(QLayout::SetNoConstraint);
    { insideLayout->addWidget(m_mypaintBrushesStylePage); }
    QFrame *insideFrame = new QFrame();
    insideFrame->setMinimumWidth(50);
    insideFrame->setLayout(insideLayout);
    m_mypaintArea = makeChooserPage(insideFrame);
    m_mypaintArea->setMinimumWidth(50);
    outsideLayout->addWidget(m_mypaintArea);

    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(2, 2, 2, 2);
    searchLayout->setSpacing(0);
    searchLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      searchLayout->addWidget(m_mypaintSearchText);
      searchLayout->addWidget(m_mypaintSearchClear);
    }
    m_mypaintSearchFrame->setLayout(searchLayout);
    outsideLayout->addWidget(m_mypaintSearchFrame);
  }
  outsideFrame->setLayout(outsideLayout);

  /* ------ signal-slot connections ------ */
  bool ret = true;
  ret =
      ret && connect(m_mypaintSearchText, SIGNAL(textChanged(const QString &)),
                     this, SLOT(onMyPaintSearch(const QString &)));
  ret = ret && connect(m_mypaintSearchClear, SIGNAL(clicked()), this,
                       SLOT(onMyPaintClearSearch()));

  assert(ret);
  return outsideFrame;
}

//-----------------------------------------------------------------------------

void StyleEditor::onTextureSearch(const QString &search) {
  m_textureSearchClear->setDisabled(search.isEmpty());
  m_textureStylePage->applyFilter(search);
  m_textureStylePage->computeSize();
}

//-----------------------------------------------------------------------------

void StyleEditor::onTextureClearSearch() {
  m_textureSearchText->setText("");
  m_textureSearchText->setFocus();
}

//-----------------------------------------------------------------------------

void StyleEditor::onVectorsSearch(const QString &search) {
  m_vectorsSearchClear->setDisabled(search.isEmpty());
  m_specialStylePage->applyFilter(search);
  m_customStylePage->applyFilter(search);
  m_vectorBrushesStylePage->applyFilter(search);
  m_specialStylePage->computeSize();
  m_customStylePage->computeSize();
  m_vectorBrushesStylePage->computeSize();
}

//-----------------------------------------------------------------------------

void StyleEditor::onVectorsClearSearch() {
  m_vectorsSearchText->setText("");
  m_vectorsSearchText->setFocus();
}

//-----------------------------------------------------------------------------

void StyleEditor::onMyPaintSearch(const QString &search) {
  m_mypaintSearchClear->setDisabled(search.isEmpty());
  m_mypaintBrushesStylePage->applyFilter(search);
  m_mypaintBrushesStylePage->computeSize();
}

//-----------------------------------------------------------------------------

void StyleEditor::onMyPaintClearSearch() {
  m_mypaintSearchText->setText("");
  m_mypaintSearchText->setFocus();
}

//-----------------------------------------------------------------------------

void StyleEditor::updateTabBar() {
  m_styleBar->clearTabBar();
  if (m_enabled && !m_enabledOnlyFirstTab && !m_enabledFirstAndLastTab) {
    m_styleBar->addSimpleTab(tr("Color"));
    m_styleBar->addSimpleTab(tr("Texture"));
    m_styleBar->addSimpleTab(tr("Vector"));
    m_styleBar->addSimpleTab(tr("Raster"));
    m_styleBar->addSimpleTab(tr("Settings"));
  } else if (m_enabled && m_enabledOnlyFirstTab && !m_enabledFirstAndLastTab)
    m_styleBar->addSimpleTab(tr("Color"));
  else if (m_enabled && !m_enabledOnlyFirstTab && m_enabledFirstAndLastTab) {
    m_styleBar->addSimpleTab(tr("Color"));
    m_styleBar->addSimpleTab(tr("Settings"));
  } else {
    m_styleChooser->setCurrentIndex(m_styleChooser->count() - 1);
    return;
  }
  m_tabBarContainer->layout()->update();
  m_styleChooser->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void StyleEditor::showEvent(QShowEvent *) {
  m_autoButton->setChecked(m_paletteController->isColorAutoApplyEnabled());
  onStyleSwitched();
  bool ret = true;
  ret      = ret && connect(m_paletteHandle, SIGNAL(colorStyleSwitched()),
                            SLOT(onStyleSwitched()));
  ret      = ret && connect(m_paletteHandle, SIGNAL(colorStyleChanged(bool)),
                            SLOT(onStyleChanged(bool)));
  ret      = ret && connect(m_paletteHandle, SIGNAL(paletteSwitched()), this,
                            SLOT(onStyleSwitched()));
  ret = ret && connect(m_paletteController, SIGNAL(checkPaletteLock()), this,
                       SLOT(checkPaletteLock()));
  if (m_cleanupPaletteHandle)
    ret =
        ret && connect(m_cleanupPaletteHandle, SIGNAL(colorStyleChanged(bool)),
                       SLOT(onCleanupStyleChanged(bool)));

  ret = ret && connect(m_paletteController, SIGNAL(colorAutoApplyEnabled(bool)),
                       this, SLOT(enableColorAutoApply(bool)));
  ret = ret && connect(m_paletteController,
                       SIGNAL(colorSampleChanged(const TPixel32 &)), this,
                       SLOT(setColorSample(const TPixel32 &)));
  m_plainColorPage->m_hsvFrame->setVisible(m_hsvAction->isChecked());
  m_plainColorPage->m_alphaFrame->setVisible(m_alphaAction->isChecked());
  m_plainColorPage->m_rgbFrame->setVisible(m_rgbAction->isChecked());
  m_hexLineEdit->setVisible(m_hexAction->isChecked());
  onSearchVisible(m_searchAction->isChecked());
  updateOrientationButton();
  assert(ret);

  applyColorPickerPrefs();
}

//-----------------------------------------------------------------------------

void StyleEditor::hideEvent(QHideEvent *) {
  disconnect(m_paletteHandle, 0, this, 0);
  if (m_cleanupPaletteHandle) disconnect(m_cleanupPaletteHandle, 0, this, 0);
  disconnect(m_paletteController, 0, this, 0);
}

//-----------------------------------------------------------------------------

void StyleEditor::updateOrientationButton() {
  if (m_plainColorPage->getIsVertical()) {
    m_toggleOrientationAction->setIcon(createQIcon("orientation_h"));
  } else {
    m_toggleOrientationAction->setIcon(createQIcon("orientation_v"));
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::updateStylePages() {
  // Refresh all pages
  m_textureStylePage->update();
  m_specialStylePage->update();
  m_customStylePage->update();
  m_vectorBrushesStylePage->update();
  m_mypaintBrushesStylePage->update();
}

//-----------------------------------------------------------------------------

void StyleEditor::onStyleSwitched() {
  TPalette *palette = getPalette();

  if (!palette) {
    // set the current page to empty
    m_styleChooser->setCurrentIndex(m_styleChooser->count() - 1);
    enable(false);
    m_colorParameterSelector->clear();
    m_oldStyle    = TColorStyleP();
    m_editedStyle = TColorStyleP();

    m_parent->setWindowTitle(tr("No Style Selected"));
    return;
  }

  int styleIndex = getStyleIndex();
  setEditedStyleToStyle(palette->getStyle(styleIndex));

  bool isStyleNull    = setStyle(m_editedStyle.getPointer());
  bool isColorInField = palette->getPaletteName() == L"EmptyColorFieldPalette";
  bool isValidIndex   = styleIndex > 0 || isColorInField;
  bool isCleanUpPalette = palette->isCleanupPalette();

  /* ------ update the status text ------ */
  if (!isStyleNull && isValidIndex) {
    QString statusText;
    // palette type
    if (isCleanUpPalette)
      statusText = tr("Cleanup ");
    else if (palette->getGlobalName() != L"")
      statusText = tr("Studio ");
    else
      statusText = tr("Level ");

    // palette name
    statusText += tr("Palette") + " : " +
                  QString::fromStdWString(palette->getPaletteName());

    // style name
    statusText += QString::fromStdWString(L" | #");
    statusText += QString::number(styleIndex);
    statusText += QString::fromStdWString(L" : " + m_editedStyle->getName());
    TPoint pickedPos = m_editedStyle->getPickedPosition().pos;
    if (pickedPos != TPoint())
      statusText +=
          QString(" (Picked from %1,%2)").arg(pickedPos.x).arg(pickedPos.y);

    m_parent->setWindowTitle(statusText);
  } else {
    m_parent->setWindowTitle(tr("Style Editor - No Valid Style Selected"));
  }
  enable(!isStyleNull && isValidIndex, isColorInField, isCleanUpPalette);

  updateStylePages();
}

//-----------------------------------------------------------------------------

void StyleEditor::onStyleChanged(bool isDragging) {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  assert(0 <= styleIndex && styleIndex < palette->getStyleCount());

  setEditedStyleToStyle(palette->getStyle(styleIndex));

  // ADD NULL CHECK
  if (!m_oldStyle || !m_editedStyle) return;

  if (!isDragging) {
    setOldStyleToStyle(
        m_editedStyle
            .getPointer());  // This line is needed for proper undo behavior
  }
  m_plainColorPage->setColor(*m_editedStyle, getColorParam());
  m_colorParameterSelector->setStyle(*m_editedStyle);
  m_settingsPage->setStyle(m_editedStyle);
  m_newColor->setStyle(*m_editedStyle, getColorParam());
  m_oldColor->setStyle(
      *m_oldStyle,
      getColorParam());  // This line is needed for proper undo behavior
  m_hexLineEdit->setStyle(*m_editedStyle, getColorParam());

  updateStylePages();
}

//-----------------------------------------------------------------------

void StyleEditor::onCleanupStyleChanged(bool isDragging) {
  if (!m_cleanupPaletteHandle) return;

  onStyleChanged(isDragging);
}

//-----------------------------------------------------------------------------
// Remove
void StyleEditor::setRootPath(const TFilePath &rootPath) {
  m_textureStylePage->setRootPath(rootPath);
}

//-----------------------------------------------------------------------------

void StyleEditor::copyEditedStyleToPalette(bool isDragging) {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  if (styleIndex < 0 || styleIndex >= palette->getStyleCount()) return;

  // CRITICAL NULL CHECKS
  if (!m_oldStyle || !m_editedStyle) {
    return;
  }

  // Safe to proceed with comparison
  if (!(*m_oldStyle == *m_editedStyle) &&
      (!isDragging || m_paletteController->isColorAutoApplyEnabled()) &&
      m_editedStyle->getGlobalName() != L"" &&
      m_editedStyle->getOriginalName() != L"") {
    // If the edited style is linked to the studio palette, then activate the
    // edited flag
    m_editedStyle->setIsEditedFlag(true);
  }

  palette->setStyle(styleIndex,
                    m_editedStyle->clone());  // Must be done *before* setting
                                              // the eventual palette keyframe
  if (!isDragging) {
    if (!(*m_oldStyle == *m_editedStyle)) {
      // do not register undo if the edited color is special one (e.g. changing
      // the ColorField in the fx settings)
      if (palette->getPaletteName() != L"EmptyColorFieldPalette")
        TUndoManager::manager()->add(new UndoPaletteChange(
            m_paletteHandle, styleIndex, *m_oldStyle, *m_editedStyle));
    }

    setOldStyleToStyle(m_editedStyle.getPointer());

    // In case the frame is a keyframe, update it
    if (palette->isKeyframe(styleIndex, palette->getFrame()))
      palette->setKeyframe(styleIndex, palette->getFrame());

    palette->setDirtyFlag(true);
  }

  m_paletteHandle->notifyColorStyleChanged(isDragging);
}

//-----------------------------------------------------------------------------

void StyleEditor::onColorChanged(const ColorModel &color, bool isDragging) {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  if (styleIndex < 0 || styleIndex > palette->getStyleCount()) return;

  setEditedStyleToStyle(palette->getStyle(styleIndex));  // CLONES the argument

  if (m_editedStyle)  // Should be styleIndex's style at this point
  {
    TPixel tColor = color.getTPixel();

    if (m_editedStyle->hasMainColor()) {
      int index = getColorParam(), count = m_editedStyle->getColorParamCount();

      if (0 <= index && index < count)
        m_editedStyle->setColorParamValue(index, tColor);
      else
        m_editedStyle->setMainColor(tColor);

      m_editedStyle->invalidateIcon();
    } else {
      // The argument has NO (main) COLOR. Since color data is being updated, a
      // 'fake'
      // solid style will be created and operated on.
      TSolidColorStyle *style = new TSolidColorStyle(tColor);
      style->assignNames(m_editedStyle.getPointer());

      setEditedStyleToStyle(style);

      delete style;
    }

    m_newColor->setStyle(*m_editedStyle, getColorParam());
    m_colorParameterSelector->setStyle(*m_editedStyle);
    m_hexLineEdit->setStyle(*m_editedStyle, getColorParam());
    // Auto Button should be disabled with locked palette
    if (m_autoButton->isEnabled() && m_autoButton->isChecked()) {
      copyEditedStyleToPalette(isDragging);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::enable(bool enabled, bool enabledOnlyFirstTab,
                         bool enabledFirstAndLastTab) {
  if (m_enabled != enabled || m_enabledOnlyFirstTab != enabledOnlyFirstTab ||
      m_enabledFirstAndLastTab != enabledFirstAndLastTab) {
    m_enabled                = enabled;
    m_enabledOnlyFirstTab    = enabledOnlyFirstTab;
    m_enabledFirstAndLastTab = enabledFirstAndLastTab;
    updateTabBar();
    m_autoButton->setEnabled(enabled);
    m_applyButton->setDisabled(!enabled || m_autoButton->isChecked());
    m_oldColor->setEnable(enabled);
    m_newColor->setEnable(enabled);
    m_hexLineEdit->setEnabled(enabled);
    if (enabled == false) {
      m_oldColor->setColor(TPixel32::Transparent);
      m_newColor->setColor(TPixel32::Transparent);
    }
  }

  // lock button behavior
  TPalette *palette = getPalette();
  if (palette && enabled) {
    // when the palette is locked
    if (palette->isLocked()) {
      m_applyButton->setEnabled(false);
      m_autoButton->setEnabled(false);
    } else  // when the palette is unlocked
    {
      m_applyButton->setDisabled(m_autoButton->isChecked());
      m_autoButton->setEnabled(true);
    }
  }
}
//-----------------------------------------------------------------------------

void StyleEditor::checkPaletteLock() {
  if (getPalette() && getPalette()->isLocked()) {
    m_applyButton->setEnabled(false);
    m_autoButton->setEnabled(false);
  } else {
    m_applyButton->setDisabled(m_autoButton->isChecked());
    m_autoButton->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onOldStyleClicked() {
  if (!m_enabled) return;
  selectStyle(*(m_oldColor->getStyle()));
}

//-----------------------------------------------------------------------------

void StyleEditor::onNewStyleClicked() { applyButtonClicked(); }

//-----------------------------------------------------------------------------

void StyleEditor::setPage(int index) {
  if (!m_enabledFirstAndLastTab) {
    m_styleChooser->setCurrentIndex(index);
    return;
  }

  // If I'm in the case where both first and last page are enabled and index ==
  // 1, the page I want to set is the last one!
  if (index == 1)
    index = m_styleChooser->count() -
            2;  // 2 because at the end there is a blank page.
  m_styleChooser->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StyleEditor::applyButtonClicked() {
  TPalette *palette = getPalette();
  int styleIndex    = getStyleIndex();
  if (!palette || styleIndex < 0 || styleIndex > palette->getStyleCount())
    return;

  copyEditedStyleToPalette(false);
}

//-----------------------------------------------------------------------------

void StyleEditor::autoCheckChanged(bool value) {
  m_paletteController->enableColorAutoApply(value);

  if (!m_enabled) return;

  m_applyButton->setDisabled(value);
}

//-----------------------------------------------------------------------------

void StyleEditor::enableColorAutoApply(bool enabled) {
  if (m_autoButton->isChecked() != enabled) {
    m_autoButton->setChecked(enabled);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::setColorSample(const TPixel32 &color) {
  // m_colorParameterSelector->setColor(*style);
  ColorModel cm;
  cm.setTPixel(color);
  onColorChanged(cm, true);
}

//-----------------------------------------------------------------------------

bool StyleEditor::setStyle(TColorStyle *currentStyle) {
  assert(currentStyle);

  bool isStyleNull = false;

  QString gname = QString::fromStdWString(currentStyle->getGlobalName());
  // if(!gname.isEmpty() && gname == "ColorFieldSimpleColor")
  //	isStyleNull = true;
  // else
  if (!gname.isEmpty() && gname[0] != L'-') {
    currentStyle = 0;
    isStyleNull  = true;
  }

  if (currentStyle) {
    m_colorParameterSelector->setStyle(*currentStyle);
    m_plainColorPage->setColor(*currentStyle, getColorParam());
    m_oldColor->setStyle(*currentStyle, getColorParam());
    m_newColor->setStyle(*currentStyle, getColorParam());
    m_hexLineEdit->setStyle(*m_editedStyle, getColorParam());
    setOldStyleToStyle(currentStyle);
  }
  // It must be done even if there is no style, because it clears the page
  m_settingsPage->setStyle(m_editedStyle);

  return isStyleNull;
}

//-----------------------------------------------------------------------------

void StyleEditor::setEditedStyleToStyle(const TColorStyle *style) {
  if (style == m_editedStyle.getPointer()) return;

  m_editedStyle = TColorStyleP(style->clone());
}

//-----------------------------------------------------------------------------

void StyleEditor::setOldStyleToStyle(const TColorStyle *style) {
  if (style == m_oldStyle.getPointer()) return;
  m_oldStyle = TColorStyleP(style->clone());
}

//-----------------------------------------------------------------------------

void StyleEditor::selectStyle(const TColorStyle &newStyle) {
  TPalette *palette = m_paletteHandle->getPalette();
  if (!palette) return;

  int styleIndex = m_paletteHandle->getStyleIndex();
  if (styleIndex < 0 || palette->getStyleCount() <= styleIndex) return;

  // Register the new previous/edited style pairs
  setOldStyleToStyle(palette->getStyle(styleIndex));
  setEditedStyleToStyle(&newStyle);

  // ADD NULL CHECK
  if (!m_oldStyle || !m_editedStyle) return;

 m_editedStyle->assignNames(
      m_oldStyle.getPointer());  // Copy original name stored in the palette

  // For convenience's sake, copy the main color from the old color, if both
  // have one
  if (m_oldStyle->hasMainColor() && m_editedStyle->hasMainColor())
    m_editedStyle->setMainColor(m_oldStyle->getMainColor());

  if (m_autoButton->isChecked()) {
    // If the edited style is linked to the studio palette, then activate the
    // edited flag
    if (m_editedStyle->getGlobalName() != L"" &&
        m_editedStyle->getOriginalName() != L"")
      m_editedStyle->setIsEditedFlag(true);

    // Apply new style, if required
    TUndoManager::manager()->add(new UndoPaletteChange(
        m_paletteHandle, styleIndex, *m_oldStyle, *m_editedStyle));

    palette->setStyle(styleIndex, m_editedStyle->clone());

    m_paletteHandle->notifyColorStyleChanged(false);
    palette->setDirtyFlag(true);
  }

  // Update editor widgets
  m_colorParameterSelector->setStyle(*m_editedStyle);
  m_newColor->setStyle(*m_editedStyle, getColorParam());
  m_plainColorPage->setColor(*m_editedStyle, getColorParam());
  m_settingsPage->setStyle(m_editedStyle);
  m_hexLineEdit->setStyle(*m_editedStyle, getColorParam());
}

//-----------------------------------------------------------------------------

void StyleEditor::onColorParamChanged() {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  if (styleIndex < 0 || palette->getStyleCount() <= styleIndex) return;

  // ADD NULL CHECK
  if (!m_oldStyle || !m_editedStyle) return;

  if (*m_oldStyle != *m_editedStyle) applyButtonClicked();

  m_paletteHandle->setStyleParamIndex(getColorParam());

  if (TColorStyle *currentStyle = palette->getStyle(styleIndex)) {
    setEditedStyleToStyle(currentStyle);

    m_colorParameterSelector->setStyle(*m_editedStyle);
    m_newColor->setStyle(*m_editedStyle, getColorParam());
    m_oldColor->setStyle(*m_editedStyle, getColorParam());
    m_plainColorPage->setColor(*m_editedStyle, getColorParam());
    m_settingsPage->setStyle(m_editedStyle);
    m_hexLineEdit->setStyle(*m_editedStyle, getColorParam());
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onParamStyleChanged(bool isDragging) {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  if (styleIndex < 0 || styleIndex > palette->getStyleCount()) return;

  if (m_autoButton->isChecked()) copyEditedStyleToPalette(isDragging);

  m_editedStyle->invalidateIcon();  // Refresh the new color icon
  m_newColor->setStyle(*m_editedStyle, getColorParam());
  m_hexLineEdit->setStyle(*m_editedStyle, getColorParam());
}

//-----------------------------------------------------------------------------

void StyleEditor::onHexChanged() {
  if (m_hexLineEdit->fromText(m_hexLineEdit->text())) {
    ColorModel cm;
    cm.setTPixel(m_hexLineEdit->getColor());
    onColorChanged(cm, false);
    m_hexLineEdit->selectAll();
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onHexEditor() {
  if (!m_hexColorNamesEditor) {
    m_hexColorNamesEditor = new DVGui::HexColorNamesEditor(this);
  }
  m_hexColorNamesEditor->show();
}

//-----------------------------------------------------------------------------

void StyleEditor::onSearchVisible(bool on) {
  m_textureSearchFrame->setVisible(on);
  m_vectorsSearchFrame->setVisible(on);
  m_mypaintSearchFrame->setVisible(on);
}

//-----------------------------------------------------------------------------

void StyleEditor::onSpecialButtonToggled(bool on) {
  m_specialStylePage->setVisible(on);
  m_vectorsArea->widget()->resize(m_vectorsArea->widget()->sizeHint());
  qApp->processEvents();
}

//-----------------------------------------------------------------------------

void StyleEditor::onCustomButtonToggled(bool on) {
  m_customStylePage->setVisible(on);
  m_vectorsArea->widget()->resize(m_vectorsArea->widget()->sizeHint());
  qApp->processEvents();
}

//-----------------------------------------------------------------------------

void StyleEditor::onVectorBrushButtonToggled(bool on) {
  m_vectorBrushesStylePage->setVisible(on);
  m_vectorsArea->widget()->resize(m_vectorsArea->widget()->sizeHint());
  qApp->processEvents();
}

//-----------------------------------------------------------------------------

void StyleEditor::save(QSettings &settings) const {
  settings.setValue("isVertical", m_plainColorPage->getIsVertical());
  int visibleParts = 0;
  if (m_pickerAction->isChecked()) visibleParts |= 0x01;
  if (m_hsvAction->isChecked()) visibleParts |= 0x02;
  if (m_alphaAction->isChecked()) visibleParts |= 0x04;
  if (m_rgbAction->isChecked()) visibleParts |= 0x08;
  if (m_hexAction->isChecked()) visibleParts |= 0x10;
  if (m_swatchAction && m_swatchAction->isChecked()) visibleParts |= 0x80;
  if (m_searchAction->isChecked()) visibleParts |= 0x20;
  settings.setValue("visibleParts", visibleParts);
  settings.setValue("splitterState", m_plainColorPage->getSplitterState());
}
void StyleEditor::load(QSettings &settings) {
  QVariant isVertical = settings.value("isVertical");
  if (isVertical.canConvert(QVariant::Bool)) {
    m_colorPageIsVertical = isVertical.toBool();
    m_plainColorPage->setIsVertical(m_colorPageIsVertical);
  }
  QVariant visibleParts = settings.value("visibleParts");
  if (visibleParts.canConvert(QVariant::Int)) {
    int visiblePartsInt = visibleParts.toInt();

    if (visiblePartsInt & 0x01)
      m_pickerAction->setChecked(true);
    else
      m_pickerAction->setChecked(false);
    if (visiblePartsInt & 0x02)
      m_hsvAction->setChecked(true);
    else
      m_hsvAction->setChecked(false);
    if (visiblePartsInt & 0x04)
      m_alphaAction->setChecked(true);
    else
      m_alphaAction->setChecked(false);
    if (visiblePartsInt & 0x08)
      m_rgbAction->setChecked(true);
    else
      m_rgbAction->setChecked(false);
    if (visiblePartsInt & 0x10)
      m_hexAction->setChecked(true);
    else
      m_hexAction->setChecked(false);
    if (m_swatchAction) {
      m_swatchAction->setChecked((visiblePartsInt & 0x80) != 0);
    }
    if (visiblePartsInt & 0x20)
      m_searchAction->setChecked(true);
    else
      m_searchAction->setChecked(false);
  }
  QVariant splitterState = settings.value("splitterState");
  if (splitterState.canConvert(QVariant::ByteArray))
    m_plainColorPage->setSplitterState(splitterState.toByteArray());
}

//-----------------------------------------------------------------------------

void StyleEditor::updateColorCalibration() {
  m_plainColorPage->updateColorCalibration();
}

//-----------------------------------------------------------------------------

void StyleEditor::onSliderAppearanceSelected(QAction *action) {
  bool ok          = true;
  int appearanceId = action->data().toInt(&ok);
  if (!ok) return;
  if (appearanceId == StyleEditorColorSliderAppearance) return;
  StyleEditorColorSliderAppearance = appearanceId;
  ColorSlider::s_slider_appearance = appearanceId;
  m_plainColorPage->update();
}

//-----------------------------------------------------------------------------

void StyleEditor::onPickerKindClicked(int kind) {
  if (kind < 0) {
    m_plainColorPage->setPickerVisible(false);
    return;
  }
  StyleEditorAdvancedPickerKind = static_cast<int>(normalizedPickerKind(kind));
  m_plainColorPage->setPickerVisible(true);
  applyColorPickerPrefs();
}

//-----------------------------------------------------------------------------

void StyleEditor::onAdvancedModeButtonClicked() {
  const ColorPageMode next =
      (m_plainColorPage->colorPageMode() == ColorPageMode::Advanced)
          ? ColorPageMode::Classic
          : ColorPageMode::Advanced;
  StyleEditorColorPageMode = static_cast<int>(next);
  applyColorPickerPrefs();
}

//-----------------------------------------------------------------------------

void StyleEditor::onSvShapeButtonClicked() {
  const AdvancedSvShape next =
      (m_plainColorPage->advancedSvShape() == AdvancedSvShape::Square)
          ? AdvancedSvShape::Triangle
          : AdvancedSvShape::Square;
  StyleEditorAdvancedSvShape = static_cast<int>(next);
  applyColorPickerPrefs();
}

//-----------------------------------------------------------------------------

void StyleEditor::applyColorPickerPrefs() {
  m_plainColorPage->setColorPageMode(
      normalizedColorPageMode((int)StyleEditorColorPageMode));
  m_plainColorPage->setAdvancedSvShape(
      normalizedSvShape((int)StyleEditorAdvancedSvShape));
  m_plainColorPage->setPickerKind(
      normalizedPickerKind((int)StyleEditorAdvancedPickerKind));
}

//-----------------------------------------------------------------------------

void StyleEditor::fillPickerContextMenu(QMenu *menu) {
  const bool advanced =
      m_plainColorPage->colorPageMode() == ColorPageMode::Advanced;
  QAction *modeAct = menu->addAction(advanced ? tr("Switch to Classic")
                                              : tr("Switch to Advanced"));
  modeAct->setData(QStringLiteral("mode:toggle"));

  if (advanced) {
    menu->addSeparator();
    const bool wheelKind =
        m_plainColorPage->pickerKind() == AdvancedPickerKind::Wheel;
    const bool pickerOn = m_plainColorPage->pickerVisible();
    QAction *wheelAct = menu->addAction(tr("Wheel"));
    wheelAct->setCheckable(true);
    wheelAct->setData(QStringLiteral("kind:wheel"));
    QAction *rectAct = menu->addAction(tr("Rectangle"));
    rectAct->setCheckable(true);
    rectAct->setData(QStringLiteral("kind:rectangle"));
    wheelAct->setChecked(wheelKind && pickerOn);
    rectAct->setChecked(!wheelKind && pickerOn);
    if (wheelKind && pickerOn) {
      menu->addSeparator();
      QAction *squareAct = menu->addAction(tr("Square"));
      squareAct->setCheckable(true);
      squareAct->setData(QStringLiteral("shape:square"));
      squareAct->setToolTip(
          tr("Show saturation and brightness inside a square"));
      QAction *triangleAct = menu->addAction(tr("Triangle"));
      triangleAct->setCheckable(true);
      triangleAct->setData(QStringLiteral("shape:triangle"));
      triangleAct->setToolTip(
          tr("Show saturation and brightness inside a triangle"));
      const bool square =
          m_plainColorPage->advancedSvShape() == AdvancedSvShape::Square;
      squareAct->setChecked(square);
      triangleAct->setChecked(!square);
    }
  }
  menu->addSeparator();
  QAction *showSections = menu->addAction(tr("Show Section Toggles"));
  showSections->setCheckable(true);
  showSections->setData(QStringLiteral("chrome:sections"));
  showSections->setChecked(StyleEditorShowSectionToggles != 0);
  if (advanced) {
    QAction *showKindBtns =
        menu->addAction(tr("Show Wheel / Rectangle Icons"));
    showKindBtns->setCheckable(true);
    showKindBtns->setData(QStringLiteral("chrome:kind"));
    showKindBtns->setChecked(StyleEditorShowPickerKindButtons != 0);
  }
  QAction *showAdvBtn = menu->addAction(tr("Show Classic / Advanced Icon"));
  showAdvBtn->setCheckable(true);
  showAdvBtn->setData(QStringLiteral("chrome:adv"));
  showAdvBtn->setChecked(StyleEditorShowAdvancedModeButton != 0);
  QAction *showVarBtn = menu->addAction(tr("Show VAR Icon"));
  showVarBtn->setCheckable(true);
  showVarBtn->setData(QStringLiteral("chrome:var"));
  showVarBtn->setChecked(StyleEditorShowVarButton != 0);
  if (advanced) {
    QAction *showShapeBtn = menu->addAction(tr("Show Chromatic Space Icon"));
    showShapeBtn->setCheckable(true);
    showShapeBtn->setData(QStringLiteral("chrome:shape"));
    showShapeBtn->setChecked(StyleEditorShowSvShapeButton != 0);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onPickerContextMenu(const QPoint &globalPos) {
  QMenu menu(this);
  fillPickerContextMenu(&menu);

  QAction *chosen = menu.exec(globalPos);
  if (!chosen) return;
  const QString key = chosen->data().toString();
  if (key == QStringLiteral("mode:toggle") ||
      key == QStringLiteral("mode:classic") ||
      key == QStringLiteral("mode:advanced")) {
    const ColorPageMode next =
        (m_plainColorPage->colorPageMode() == ColorPageMode::Advanced)
            ? ColorPageMode::Classic
            : ColorPageMode::Advanced;
    StyleEditorColorPageMode = static_cast<int>(next);
    applyColorPickerPrefs();
  } else if (key == QStringLiteral("kind:wheel")) {
    if (!chosen->isChecked()) {
      m_plainColorPage->setPickerVisible(false);
    } else {
      StyleEditorAdvancedPickerKind =
          static_cast<int>(AdvancedPickerKind::Wheel);
      m_plainColorPage->setPickerVisible(true);
      applyColorPickerPrefs();
    }
  } else if (key == QStringLiteral("kind:rectangle")) {
    if (!chosen->isChecked()) {
      m_plainColorPage->setPickerVisible(false);
    } else {
      StyleEditorAdvancedPickerKind =
          static_cast<int>(AdvancedPickerKind::Rectangle);
      m_plainColorPage->setPickerVisible(true);
      applyColorPickerPrefs();
    }
  } else if (key == QStringLiteral("shape:square")) {
    StyleEditorAdvancedSvShape = static_cast<int>(AdvancedSvShape::Square);
    applyColorPickerPrefs();
  } else if (key == QStringLiteral("shape:triangle")) {
    StyleEditorAdvancedSvShape = static_cast<int>(AdvancedSvShape::Triangle);
    applyColorPickerPrefs();
  } else if (key == QStringLiteral("chrome:adv")) {
    StyleEditorShowAdvancedModeButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:shape")) {
    StyleEditorShowSvShapeButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:kind")) {
    StyleEditorShowPickerKindButtons = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:sections")) {
    StyleEditorShowSectionToggles = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:var")) {
    StyleEditorShowVarButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onPopupMenuAboutToShow() {
  // sync radio button state to the current user env settings
  for (auto action : m_sliderAppearanceAG->actions()) {
    bool ok          = true;
    int appearanceId = action->data().toInt(&ok);
    if (ok && appearanceId == StyleEditorColorSliderAppearance)
      action->setChecked(true);
  }
}
