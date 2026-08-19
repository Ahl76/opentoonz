

#include "toonzqt/styleeditor.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/filefield.h"
#include "toonzqt/menubarcommand.h"
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
#include "tenv.h"
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
#include <QImage>
#include <QPixmap>
#include <QButtonGroup>
#include <QAbstractButton>
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
#include <QHelpEvent>
#include <QSplitter>
#include <QMenu>
#include <QStringList>
#include <QOpenGLFramebufferObject>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QFocusEvent>
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
TEnv::IntVar StyleEditorShowColorFeaturesBar("StyleEditorShowColorFeaturesBar", 1);
TEnv::IntVar StyleEditorShowCollectorButton("StyleEditorShowCollectorButton",
                                            1);
TEnv::IntVar StyleEditorShowHistoryButton("StyleEditorShowHistoryButton", 1);
TEnv::StringVar StyleEditorColorCollector("StyleEditorColorCollector", "");
TEnv::StringVar StyleEditorColorHistory("StyleEditorColorHistory", "");
TEnv::IntVar StyleEditorShowHarmonyButton("StyleEditorShowHarmonyButton", 1);
TEnv::IntVar StyleEditorHarmonyCut("StyleEditorHarmonyCut", 0);
TEnv::IntVar StyleEditorShowShadesButton("StyleEditorShowShadesButton", 1);
TEnv::IntVar StyleEditorShowMixerButton("StyleEditorShowMixerButton", 1);
TEnv::IntVar StyleEditorShowNeighborsButton("StyleEditorShowNeighborsButton", 1);
TEnv::IntVar StyleEditorShowBlendButton("StyleEditorShowBlendButton", 1);
TEnv::IntVar StyleEditorNeighborsDenseGrid("StyleEditorNeighborsDenseGrid", 0);
TEnv::IntVar StyleEditorBlendDenseGrid("StyleEditorBlendDenseGrid", 0);
TEnv::IntVar StyleEditorNeighborsHAxis("StyleEditorNeighborsHAxis",
                                       static_cast<int>(StyleEditorGUI::eHue));
TEnv::IntVar StyleEditorNeighborsVAxis("StyleEditorNeighborsVAxis",
                                       static_cast<int>(StyleEditorGUI::eValue));
TEnv::IntVar StyleEditorNeighborsHPct("StyleEditorNeighborsHPct", 30);
TEnv::IntVar StyleEditorNeighborsVPct("StyleEditorNeighborsVPct", 40);
TEnv::IntVar StyleEditorMixerPaintMix("StyleEditorMixerPaintMix", 1);
TEnv::IntVar StyleEditorMixerBg("StyleEditorMixerBg", 0);
TEnv::StringVar StyleEditorColorBlend0("StyleEditorColorBlend0", "");
TEnv::StringVar StyleEditorColorBlend1("StyleEditorColorBlend1", "");
TEnv::StringVar StyleEditorColorBlend2("StyleEditorColorBlend2", "");
TEnv::StringVar StyleEditorColorBlend3("StyleEditorColorBlend3", "");
TEnv::StringVar StyleEditorColorRampA("StyleEditorColorRampA", "");
TEnv::StringVar StyleEditorColorRampB("StyleEditorColorRampB", "");

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

enum HarmonyCut { HarmonyNone = 0, HarmonyComplementary = 1,
                  HarmonyAnalogous = 2, HarmonyTetrad = 3 };

HarmonyCut normalizedHarmonyCut(int id) {
  if (id == HarmonyComplementary || id == HarmonyAnalogous ||
      id == HarmonyTetrad)
    return static_cast<HarmonyCut>(id);
  return HarmonyNone;
}

enum MixerBlend {
  MixerRgb     = 0,
  MixerRyb = 1,
  MixerFinger  = 2,
  MixerSoft    = 3
};

MixerBlend normalizedMixerBlend(int id) {
  if (id == MixerRgb || id == MixerFinger || id == MixerSoft)
    return static_cast<MixerBlend>(id);
  return MixerRyb;
}

int wrapHue(int h) {
  h %= 360;
  if (h < 0) h += 360;
  return h;
}

int harmonyHueCount(HarmonyCut cut) {
  if (cut == HarmonyComplementary) return 2;
  if (cut == HarmonyAnalogous) return 3;
  if (cut == HarmonyTetrad) return 4;
  return 1;
}

void fillHarmonyHues(int hue, HarmonyCut cut, int *out) {
  hue    = wrapHue(hue);
  out[0] = hue;
  if (cut == HarmonyComplementary)
    out[1] = wrapHue(hue + 180);
  else if (cut == HarmonyAnalogous) {
    out[1] = wrapHue(hue + 45);
    out[2] = wrapHue(hue - 45);
  } else if (cut == HarmonyTetrad) {
    out[1] = wrapHue(hue + 180);
    out[2] = wrapHue(hue + 45);
    out[3] = wrapHue(hue + 225);
  }
}

ColorModel colorAtHue(const ColorModel &src, int hue) {
  ColorModel c = src;
  c.setValue(eHue, wrapHue(hue));
  return c;
}

int lerpHue(int a, int b, double t) {
  int d = b - a;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return wrapHue(a + (int)std::lround(d * t));
}

ColorModel colorAtValue(const ColorModel &src, int v) {
  ColorModel c = src;
  c.setValue(eValue, qBound(0, v, 100));
  return c;
}

ColorModel colorAtTemperature(const ColorModel &src, double t) {
  ColorModel c     = src;
  const int target = t < 0 ? 220 : 40;
  const double amt = std::min(1.0, std::fabs(t)) * 0.7;
  if (amt <= 0) return c;
  c.setValue(eHue, lerpHue(src.getValue(eHue), target, amt));
  return c;
}

ColorModel colorAtSaturation(const ColorModel &src, int s) {
  ColorModel c = src;
  c.setValue(eSaturation, qBound(0, s, 100));
  return c;
}

ColorModel lerpHsv(const ColorModel &a, const ColorModel &b, double t) {
  ColorModel c = a;
  t            = qBound(0.0, t, 1.0);
  c.setValue(eHue, lerpHue(a.getValue(eHue), b.getValue(eHue), t));
  c.setValue(eSaturation,
             qBound(0,
                    (int)std::lround(a.getValue(eSaturation) +
                                     (b.getValue(eSaturation) -
                                      a.getValue(eSaturation)) *
                                         t),
                    100));
  c.setValue(eValue, qBound(0,
                            (int)std::lround(a.getValue(eValue) +
                                             (b.getValue(eValue) -
                                              a.getValue(eValue)) *
                                                 t),
                            100));
  c.setValue(eAlpha, qBound(0,
                            (int)std::lround(a.getValue(eAlpha) +
                                             (b.getValue(eAlpha) -
                                              a.getValue(eAlpha)) *
                                                 t),
                            255));
  return c;
}

ColorModel lerpRgb(const ColorModel &a, const ColorModel &b, double t) {
  t                 = qBound(0.0, t, 1.0);
  const TPixel32 pa = a.getTPixel();
  const TPixel32 pb = b.getTPixel();
  ColorModel c;
  c.setTPixel(TPixel32(
      (UCHAR)std::lround(pa.r + (pb.r - pa.r) * t),
      (UCHAR)std::lround(pa.g + (pb.g - pa.g) * t),
      (UCHAR)std::lround(pa.b + (pb.b - pa.b) * t),
      (UCHAR)std::lround(pa.m + (pb.m - pa.m) * t)));
  return c;
}

bool colorFromEnv(const std::string &raw, ColorModel *out) {
  const QColor qc(QString::fromStdString(raw).trimmed());
  if (!qc.isValid()) return false;
  out->setTPixel(TPixel32((UCHAR)qc.red(), (UCHAR)qc.green(), (UCHAR)qc.blue(),
                          (UCHAR)qc.alpha()));
  return true;
}

std::string colorToEnv(const ColorModel &c) {
  const TPixel32 p = c.getTPixel();
  return QColor(p.r, p.g, p.b, p.m).name(QColor::HexArgb).toStdString();
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
  drawHarmonyMarks();
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

void HexagonalColorWheel::drawHarmonyMarks() {
  const HarmonyCut cut = normalizedHarmonyCut(StyleEditorHarmonyCut);
  if (cut == HarmonyNone) return;
  int hues[4];
  fillHarmonyHues(m_color.getValue(eHue), cut, hues);
  const int n = harmonyHueCount(cut);
  for (int i = 1; i < n; ++i) {
    const int hue = hues[i];
    if (m_pageMode == ColorPageMode::Classic) {
      const int h = 360 - hue;
      glPushMatrix();
      float phi = (float)(h % 60 - 30) / 180.0f * 3.1415f;
      float d   = m_hexTriHeight / cosf(phi);
      glTranslatef(m_wp[0].x(), m_wp[0].y(), 0.1f);
      glRotatef(h, 0.0, 0.0, 1.0);
      glTranslatef(d, 0.0f, 0.0f);
      glRotatef(-h, 0.0, 0.0, 1.0);
      drawColorCursor(0.0f, 0.0f);
      glPopMatrix();
    } else {
      drawHueRingBaton(hue, m_innerRadius, m_outerRadius, m_circleCenter);
    }
  }
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

  if (m_channel == eHue) {
    const HarmonyCut cut = normalizedHarmonyCut(StyleEditorHarmonyCut);
    if (cut != HarmonyNone) {
      int hues[4];
      fillHarmonyHues(m_color.getValue(eHue), cut, hues);
      const int n   = harmonyHueCount(cut);
      const int max = maximum();
      p.save();
      for (int i = 1; i < n; ++i) {
        const int pos = QStyle::sliderPositionFromValue(0, max, hues[i],
                                                        isVertical ? h : w,
                                                        isVertical);
        p.setPen(QPen(QColor(20, 20, 20), 3));
        if (isVertical)
          p.drawLine(x, y + pos, x + w, y + pos);
        else
          p.drawLine(x + pos, y, x + pos, y + h);
        p.setPen(QPen(Qt::white, 1));
        if (isVertical)
          p.drawLine(x, y + pos, x + w, y + pos);
        else
          p.drawLine(x + pos, y, x + pos, y + h);
      }
      p.restore();
    }
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

class ColorCollectorGrid final : public QWidget {
  static const int kCols = 12;
  static const int kRows = 6;
  static const int kCount = kCols * kRows;
  static const int kGap   = 2;
  ColorModel m_slots[kCount];
  bool m_filled[kCount];
  std::function<ColorModel()> m_current;
  std::function<void(const ColorModel &)> m_pick;

  QRect cellRect(int i) const {
    const QRect area = contentsRect().adjusted(kGap, kGap, -kGap, -kGap);
    if (area.width() <= 0 || area.height() <= 0) return QRect();
    const int r = i / kCols;
    const int c = i % kCols;
    const int cw = (area.width() - kGap * (kCols - 1)) / kCols;
    const int ch = (area.height() - kGap * (kRows - 1)) / kRows;
    if (cw <= 0 || ch <= 0) return QRect();
    return QRect(area.x() + c * (cw + kGap), area.y() + r * (ch + kGap), cw,
                 ch);
  }

  int hit(const QPoint &p) const {
    for (int i = 0; i < kCount; ++i)
      if (cellRect(i).contains(p)) return i;
    return -1;
  }

  void persist() {
    QStringList parts;
    parts.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
      if (!m_filled[i]) {
        parts.append(QStringLiteral("-"));
        continue;
      }
      const TPixel32 p = m_slots[i].getTPixel();
      parts.append(QColor(p.r, p.g, p.b, p.m).name(QColor::HexArgb));
    }
    StyleEditorColorCollector = parts.join(QLatin1Char(',')).toStdString();
  }

  void restore() {
    const QString raw =
        QString::fromStdString((std::string)StyleEditorColorCollector);
    const QStringList parts = raw.split(QLatin1Char(','));
    for (int i = 0; i < kCount; ++i) {
      m_filled[i] = false;
      m_slots[i]  = ColorModel();
      if (i >= parts.size()) continue;
      const QString s = parts[i].trimmed();
      if (s.isEmpty() || s == QLatin1String("-")) continue;
      const QColor qc(s);
      if (!qc.isValid()) continue;
      m_slots[i].setTPixel(TPixel32((UCHAR)qc.red(), (UCHAR)qc.green(),
                                    (UCHAR)qc.blue(), (UCHAR)qc.alpha()));
      m_filled[i] = true;
    }
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    for (int i = 0; i < kCount; ++i) {
      const QRect r = cellRect(i);
      if (!r.isValid()) continue;
      if (m_filled[i]) {
        const TPixel32 pix = m_slots[i].getTPixel();
        p.fillRect(r, QColor(pix.r, pix.g, pix.b, pix.m));
      } else {
        p.fillRect(r, palette().mid());
      }
      p.setPen(QPen(palette().mid(), 1));
      p.drawRect(r.adjusted(0, 0, -1, -1));
    }
  }

  void collectAt(int i) {
    if (!m_current) return;
    m_slots[i]  = m_current();
    m_filled[i] = true;
    persist();
    update();
  }

  void mousePressEvent(QMouseEvent *e) override {
    const int i = hit(e->pos());
    if (i < 0 || e->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(e);
      return;
    }
    if (e->modifiers() & Qt::AltModifier) {
      collectAt(i);
      e->accept();
      return;
    }
    if (m_filled[i] && m_pick) m_pick(m_slots[i]);
    e->accept();
  }

  bool event(QEvent *e) override {
    if (e->type() == QEvent::ToolTip) {
      QHelpEvent *he = static_cast<QHelpEvent *>(e);
      const int i    = hit(he->pos());
      if (i >= 0)
        QToolTip::showText(
            he->globalPos(),
            m_filled[i] ? tr("Apply") : tr("Collect"), this);
      else
        QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }

public:
  explicit ColorCollectorGrid(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    for (int i = 0; i < kCount; ++i) m_filled[i] = false;
    restore();
  }

  void setCurrent(std::function<ColorModel()> cb) { m_current = std::move(cb); }
  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }
  void append(const ColorModel &c) {
    for (int i = 0; i < kCount; ++i) {
      if (m_filled[i]) continue;
      m_slots[i]  = c;
      m_filled[i] = true;
      persist();
      update();
      return;
    }
  }
};

class ColorHistoryGrid final : public QWidget {
  static const int kCols  = 12;
  static const int kRows  = 6;
  static const int kCount = kCols * kRows;
  static const int kGap   = 2;
  ColorModel m_slots[kCount];
  int m_used = 0;
  std::function<void(const ColorModel &)> m_pick;

  QRect cellRect(int i) const {
    const QRect area = contentsRect().adjusted(kGap, kGap, -kGap, -kGap);
    if (area.width() <= 0 || area.height() <= 0) return QRect();
    const int r  = i / kCols;
    const int c  = i % kCols;
    const int cw = (area.width() - kGap * (kCols - 1)) / kCols;
    const int ch = (area.height() - kGap * (kRows - 1)) / kRows;
    if (cw <= 0 || ch <= 0) return QRect();
    return QRect(area.x() + c * (cw + kGap), area.y() + r * (ch + kGap), cw,
                 ch);
  }

  int hit(const QPoint &p) const {
    for (int i = 0; i < kCount; ++i)
      if (cellRect(i).contains(p)) return i;
    return -1;
  }

  void persist() {
    QStringList parts;
    parts.reserve(m_used);
    for (int i = 0; i < m_used; ++i) {
      const TPixel32 p = m_slots[i].getTPixel();
      parts.append(QColor(p.r, p.g, p.b, p.m).name(QColor::HexArgb));
    }
    StyleEditorColorHistory = parts.join(QLatin1Char(',')).toStdString();
  }

  void restore() {
    m_used              = 0;
    const QString raw =
        QString::fromStdString((std::string)StyleEditorColorHistory);
    const QStringList parts = raw.split(QLatin1Char(','));
    for (const QString &s : parts) {
      if (m_used >= kCount) break;
      const QColor qc(s.trimmed());
      if (!qc.isValid()) continue;
      m_slots[m_used].setTPixel(TPixel32((UCHAR)qc.red(), (UCHAR)qc.green(),
                                         (UCHAR)qc.blue(), (UCHAR)qc.alpha()));
      ++m_used;
    }
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    for (int i = 0; i < kCount; ++i) {
      const QRect r = cellRect(i);
      if (!r.isValid()) continue;
      if (i < m_used) {
        const TPixel32 pix = m_slots[i].getTPixel();
        p.fillRect(r, QColor(pix.r, pix.g, pix.b, pix.m));
      } else {
        p.fillRect(r, palette().mid());
      }
      p.setPen(QPen(palette().mid(), 1));
      p.drawRect(r.adjusted(0, 0, -1, -1));
    }
  }

  void mousePressEvent(QMouseEvent *e) override {
    const int i = hit(e->pos());
    if (i < 0 || e->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(e);
      return;
    }
    if (i < m_used && m_pick) m_pick(m_slots[i]);
    e->accept();
  }

  bool event(QEvent *e) override {
    if (e->type() == QEvent::ToolTip) {
      QHelpEvent *he = static_cast<QHelpEvent *>(e);
      const int i    = hit(he->pos());
      if (i >= 0 && i < m_used)
        QToolTip::showText(he->globalPos(), tr("Apply"), this);
      else
        QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }

public:
  explicit ColorHistoryGrid(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    restore();
  }

  void push(const ColorModel &c) {
    const TPixel32 pix = c.getTPixel();
    if (m_used > 0 && m_slots[0].getTPixel() == pix) return;
    const int n = std::min(m_used, kCount - 1);
    for (int i = n; i > 0; --i) m_slots[i] = m_slots[i - 1];
    m_slots[0] = c;
    if (m_used < kCount) ++m_used;
    persist();
    update();
  }

  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }
};

class ColorHarmonyPane final : public QWidget {
  static const int kGap = 3;
  QButtonGroup *m_cuts;
  QToolButton *m_cutBtn[4];
  ColorModel m_src;
  std::function<void(const ColorModel &)> m_pick;
  std::function<void()> m_onCut;

  HarmonyCut cut() const {
    return normalizedHarmonyCut(StyleEditorHarmonyCut);
  }

  QRect chipRect(int i, int n) const {
    const QRect area = contentsRect().adjusted(4, 26, -4, -4);
    if (n <= 0 || area.width() <= 0 || area.height() <= 0) return QRect();
    const int cw = (area.width() - kGap * (n - 1)) / n;
    if (cw <= 0) return QRect();
    return QRect(area.x() + i * (cw + kGap), area.y(), cw, area.height());
  }

  int hit(const QPoint &p) const {
    const int n = harmonyHueCount(cut());
    for (int i = 0; i < n; ++i)
      if (chipRect(i, n).contains(p)) return i;
    return -1;
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    const HarmonyCut c = cut();
    const int n        = harmonyHueCount(c);
    int hues[4];
    fillHarmonyHues(m_src.getValue(eHue), c, hues);
    for (int i = 0; i < n; ++i) {
      const QRect r = chipRect(i, n);
      if (!r.isValid()) continue;
      const TPixel32 pix = colorAtHue(m_src, hues[i]).getTPixel();
      p.fillRect(r, QColor(pix.r, pix.g, pix.b, pix.m));
      p.setPen(QPen(palette().mid(), 1));
      p.drawRect(r.adjusted(0, 0, -1, -1));
    }
  }

  void mousePressEvent(QMouseEvent *e) override {
    const int i = hit(e->pos());
    if (i < 0 || e->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(e);
      return;
    }
    if (m_pick) {
      int hues[4];
      fillHarmonyHues(m_src.getValue(eHue), cut(), hues);
      m_pick(colorAtHue(m_src, hues[i]));
    }
    e->accept();
  }

  bool event(QEvent *e) override {
    if (e->type() == QEvent::ToolTip) {
      QHelpEvent *he = static_cast<QHelpEvent *>(e);
      const int i    = hit(he->pos());
      if (i >= 0)
        QToolTip::showText(he->globalPos(), tr("Apply"), this);
      else
        QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }

public:
  explicit ColorHarmonyPane(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    m_cuts = new QButtonGroup(this);
    m_cuts->setExclusive(true);
    const char *icons[4] = {"colorpicker_harmony_none",
                            "colorpicker_harmony_comp",
                            "colorpicker_harmony_analog",
                            "colorpicker_harmony_tetrad"};
    for (int i = 0; i < 4; ++i) {
      m_cutBtn[i] = new QToolButton(this);
      m_cutBtn[i]->setCheckable(true);
      m_cutBtn[i]->setAutoRaise(true);
      m_cutBtn[i]->setFocusPolicy(Qt::NoFocus);
      m_cutBtn[i]->setFixedSize(20, 20);
      m_cutBtn[i]->setIconSize(QSize(16, 16));
      m_cutBtn[i]->setIcon(createQIcon(icons[i]));
      m_cuts->addButton(m_cutBtn[i], i);
    }
    m_cutBtn[0]->setToolTip(tr("No cut"));
    m_cutBtn[1]->setToolTip(tr("Complementary"));
    m_cutBtn[2]->setToolTip(tr("Analogous"));
    m_cutBtn[3]->setToolTip(tr("Double complementary"));
    const int cur = (int)normalizedHarmonyCut(StyleEditorHarmonyCut);
    m_cutBtn[cur]->setChecked(true);
    connect(m_cuts, static_cast<void (QButtonGroup::*)(int)>(
                        &QButtonGroup::buttonClicked),
            this, [this](int id) {
              StyleEditorHarmonyCut = id;
              update();
              if (m_onCut) m_onCut();
            });
  }

  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    for (int i = 0; i < 4; ++i) m_cutBtn[i]->move(4 + i * 22, 3);
  }

  void setFrom(const ColorModel &color) {
    m_src = color;
    update();
  }
  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }
  void setOnCut(std::function<void()> cb) { m_onCut = std::move(cb); }
};

class ColorShadesPane final : public QWidget {
  static const int kGap    = 2;
  static const int kRowGap = 3;
  static const int kCols   = 11;
  static const int kRows   = 4;
  ColorModel m_src;
  ColorModel m_rampA;
  ColorModel m_rampB;
  bool m_hasA = false;
  bool m_hasB = false;
  std::function<void(const ColorModel &)> m_pick;
  std::function<void(const ColorModel &)> m_collect;

  enum Row { RowValue = 0, RowTemp = 1, RowGray = 2, RowRamp = 3 };

  struct Hit {
    int row = -1;
    int i   = -1;
  };
  Hit m_hover;

  QRect rowRect(int row) const {
    const QRect area = contentsRect().adjusted(4, 4, -4, -4);
    if (area.width() <= 0 || area.height() <= 0) return QRect();
    const int h = (area.height() - kRowGap * (kRows - 1)) / kRows;
    if (h <= 0) return QRect();
    return QRect(area.x(), area.y() + row * (h + kRowGap), area.width(), h);
  }

  QRect chipRect(int row, int i) const {
    const QRect area = rowRect(row);
    if (!area.isValid()) return QRect();
    const int cw = (area.width() - kGap * (kCols - 1)) / kCols;
    if (cw <= 0) return QRect();
    return QRect(area.x() + i * (cw + kGap), area.y(), cw, area.height());
  }

  Hit hit(const QPoint &p) const {
    for (int row = 0; row < kRows; ++row) {
      for (int i = 0; i < kCols; ++i)
        if (chipRect(row, i).contains(p)) return {row, i};
    }
    return {};
  }

  int valueAt(int i) const { return 14 + i * (100 - 14) / (kCols - 1); }

  int satAt(int i) const { return i * 100 / (kCols - 1); }

  ColorModel valueColor(int i) const { return colorAtValue(m_src, valueAt(i)); }

  ColorModel tempColor(int i) const {
    const int mid = kCols / 2;
    if (i == mid) return m_src;
    const double t = (double)(i - mid) / (double)mid;
    return colorAtTemperature(m_src, t);
  }

  ColorModel grayColor(int i) const {
    return colorAtSaturation(m_src, satAt(i));
  }

  ColorModel rampColor(int i) const {
    if (i == 0) return m_rampA;
    if (i == kCols - 1) return m_rampB;
    return lerpHsv(m_rampA, m_rampB, (double)i / (double)(kCols - 1));
  }

  bool rampFilled(int i) const {
    if (i == 0) return m_hasA;
    if (i == kCols - 1) return m_hasB;
    return m_hasA && m_hasB;
  }

  int closestValueChip() const {
    const int srcV = m_src.getValue(eValue);
    int best = 0, bestD = 1000;
    for (int i = 0; i < kCols; ++i) {
      const int d = std::abs(valueAt(i) - srcV);
      if (d < bestD) {
        bestD = d;
        best  = i;
      }
    }
    return best;
  }

  int closestGrayChip() const {
    const int srcS = m_src.getValue(eSaturation);
    int best = 0, bestD = 1000;
    for (int i = 0; i < kCols; ++i) {
      const int d = std::abs(satAt(i) - srcS);
      if (d < bestD) {
        bestD = d;
        best  = i;
      }
    }
    return best;
  }

  void persistRamp() {
    StyleEditorColorRampA = m_hasA ? colorToEnv(m_rampA) : std::string();
    StyleEditorColorRampB = m_hasB ? colorToEnv(m_rampB) : std::string();
  }

  void restoreRamp() {
    m_hasA = colorFromEnv((std::string)StyleEditorColorRampA, &m_rampA);
    m_hasB = colorFromEnv((std::string)StyleEditorColorRampB, &m_rampB);
  }

  void drawChip(QPainter &p, const QRect &r, const QColor &fill, bool mark,
                bool well) {
    p.fillRect(r, fill);
    QPen pen(palette().mid(), well ? 2 : 1);
    p.setPen(pen);
    p.drawRect(r.adjusted(0, 0, -1, -1));
    if (!mark) return;
    p.setPen(QPen(QColor(255, 255, 255), 1));
    p.drawRect(r.adjusted(1, 1, -2, -2));
    p.setPen(QPen(QColor(0, 0, 0), 1));
    p.drawRect(r.adjusted(2, 2, -3, -3));
  }

  void drawOutlinedText(QPainter &p, const QRect &r, const QString &s) {
    const int align = Qt::AlignHCenter | Qt::AlignVCenter;
    p.setPen(QColor(0, 0, 0));
    for (int dx = -1; dx <= 1; ++dx)
      for (int dy = -1; dy <= 1; ++dy)
        if (dx || dy) p.drawText(r.translated(dx, dy), align, s);
    p.setPen(QColor(255, 255, 255));
    p.drawText(r, align, s);
  }

  void drawHoverLabel(QPainter &p) {
    if (m_hover.row < 0) return;
    const QString title  = rowTip(m_hover.row);
    const QString action = actionTip(m_hover);
    QFont f              = font();
    f.setPixelSize(10);
    p.setFont(f);
    const QFontMetrics fm(f);
    const int w =
        std::max(fm.horizontalAdvance(title), fm.horizontalAdvance(action));
    const int lh    = fm.height();
    const int lines = action.isEmpty() ? 1 : 2;
    const int blockH = lines * lh;
    const QRect chip = chipRect(m_hover.row, m_hover.i);
    if (!chip.isValid()) return;
    int x = chip.center().x() - w / 2;
    int y = chip.bottom() + 4;
    if (y + blockH > height() - 2) y = chip.top() - blockH - 2;
    x = qBound(2, x, std::max(2, width() - w - 2));
    y = qBound(2, y, std::max(2, height() - blockH - 2));
    drawOutlinedText(p, QRect(x, y, w, lh), title);
    if (!action.isEmpty())
      drawOutlinedText(p, QRect(x, y + lh, w, lh), action);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    const int curV = closestValueChip();
    const int curS = closestGrayChip();
    const int midT = kCols / 2;
    for (int i = 0; i < kCols; ++i) {
      const QRect r = chipRect(RowValue, i);
      if (!r.isValid()) continue;
      const TPixel32 pix = valueColor(i).getTPixel();
      drawChip(p, r, QColor(pix.r, pix.g, pix.b, pix.m), i == curV, false);
    }
    for (int i = 0; i < kCols; ++i) {
      const QRect r = chipRect(RowTemp, i);
      if (!r.isValid()) continue;
      const TPixel32 pix = tempColor(i).getTPixel();
      drawChip(p, r, QColor(pix.r, pix.g, pix.b, pix.m), i == midT, false);
    }
    for (int i = 0; i < kCols; ++i) {
      const QRect r = chipRect(RowGray, i);
      if (!r.isValid()) continue;
      const TPixel32 pix = grayColor(i).getTPixel();
      drawChip(p, r, QColor(pix.r, pix.g, pix.b, pix.m), i == curS, false);
    }
    for (int i = 0; i < kCols; ++i) {
      const QRect r = chipRect(RowRamp, i);
      if (!r.isValid()) continue;
      const bool well = (i == 0 || i == kCols - 1);
      if (!rampFilled(i)) {
        p.fillRect(r, palette().mid());
        QPen pen(palette().mid(), well ? 2 : 1);
        p.setPen(pen);
        p.drawRect(r.adjusted(0, 0, -1, -1));
        continue;
      }
      const TPixel32 pix = rampColor(i).getTPixel();
      drawChip(p, r, QColor(pix.r, pix.g, pix.b, pix.m), false, well);
    }
    drawHoverLabel(p);
  }

  void mousePressEvent(QMouseEvent *e) override {
    const Hit h = hit(e->pos());
    if (h.row == RowRamp && (h.i == 0 || h.i == kCols - 1) &&
        (e->modifiers() & Qt::AltModifier)) {
      if (e->button() == Qt::RightButton) {
        if (h.i == 0)
          m_hasA = false;
        else
          m_hasB = false;
        persistRamp();
        update();
        e->accept();
        return;
      }
      if (e->button() == Qt::LeftButton) {
        if (h.i == 0) {
          m_rampA = m_src;
          m_hasA  = true;
        } else {
          m_rampB = m_src;
          m_hasB  = true;
        }
        persistRamp();
        update();
        e->accept();
        return;
      }
    }
    if (h.row < 0 || e->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(e);
      return;
    }
    ColorModel chosen;
    bool ok = false;
    if (h.row == RowValue) {
      chosen = valueColor(h.i);
      ok     = true;
    } else if (h.row == RowTemp) {
      chosen = tempColor(h.i);
      ok     = true;
    } else if (h.row == RowGray) {
      chosen = grayColor(h.i);
      ok     = true;
    } else if (rampFilled(h.i)) {
      chosen = rampColor(h.i);
      ok     = true;
    }
    if (!ok) {
      e->accept();
      return;
    }
    if ((e->modifiers() & Qt::ControlModifier) && m_collect)
      m_collect(chosen);
    else if (m_pick)
      m_pick(chosen);
    e->accept();
  }

  void mouseMoveEvent(QMouseEvent *e) override {
    const Hit h = hit(e->pos());
    if (h.row == m_hover.row && h.i == m_hover.i) return;
    m_hover = h;
    update();
  }

  void leaveEvent(QEvent *e) override {
    if (m_hover.row >= 0) {
      m_hover = {};
      update();
    }
    QWidget::leaveEvent(e);
  }

  QString rowTip(int row) const {
    if (row == RowValue) return tr("Value");
    if (row == RowTemp) return tr("Temperature");
    if (row == RowGray) return tr("Saturation");
    return tr("Ramp");
  }

  QString actionTip(const Hit &h) const {
    if (h.row == RowValue) return tr("Apply");
    if (h.row == RowTemp) {
      const int mid = kCols / 2;
      if (h.i < mid) return tr("Cool");
      if (h.i > mid) return tr("Warm");
      return tr("Apply");
    }
    if (h.row == RowGray) {
      if (h.i < closestGrayChip()) return tr("Mute");
      return tr("Apply");
    }
    if (h.row == RowRamp && (h.i == 0 || h.i == kCols - 1))
      return rampFilled(h.i) ? tr("Apply") : tr("Set");
    if (h.row == RowRamp && rampFilled(h.i)) return tr("Apply");
    return QString();
  }

  bool event(QEvent *e) override {
    if (e->type() == QEvent::ToolTip) {
      QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }

  void contextMenuEvent(QContextMenuEvent *e) override {
    if (e->modifiers() & Qt::AltModifier) {
      e->accept();
      return;
    }
    if (!m_hasA && !m_hasB) {
      QWidget::contextMenuEvent(e);
      return;
    }
    QMenu menu(this);
    QAction *resetRamp = menu.addAction(tr("Reset ramp"));
    if (menu.exec(e->globalPos()) == resetRamp) {
      m_hasA = false;
      m_hasB = false;
      persistRamp();
      update();
    }
  }

public:
  explicit ColorShadesPane(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    restoreRamp();
  }

  void setFrom(const ColorModel &color) {
    m_src = color;
    update();
  }
  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }
  void setCollect(std::function<void(const ColorModel &)> cb) {
    m_collect = std::move(cb);
  }
};

class ColorNeighborsPane final : public QWidget {
  static const int kGap = 2;
  ColorModel m_src;
  std::function<void(const ColorModel &)> m_pick;
  std::function<void(const ColorModel &)> m_collect;
  bool m_dense          = false;
  int m_cols            = 9;
  int m_rows            = 5;
  ColorChannel m_hAxis  = eHue;
  ColorChannel m_vAxis  = eValue;
  int m_hPct            = 30;
  int m_vPct            = 40;
  QSlider *m_hSlider    = nullptr;
  QSlider *m_vSlider    = nullptr;
  QLabel *m_hLab        = nullptr;
  QLabel *m_vLab        = nullptr;

  static ColorChannel normAxis(int v, ColorChannel fallback) {
    if (v == (int)eRed || v == (int)eGreen || v == (int)eBlue ||
        v == (int)eHue || v == (int)eSaturation || v == (int)eValue)
      return (ColorChannel)v;
    return fallback;
  }

  static QString axisLetter(ColorChannel ch) {
    if (ch == eHue) return QStringLiteral("H");
    if (ch == eSaturation) return QStringLiteral("S");
    if (ch == eValue) return QStringLiteral("V");
    if (ch == eRed) return QStringLiteral("R");
    if (ch == eGreen) return QStringLiteral("G");
    return QStringLiteral("B");
  }

  static int axisDelta(ColorChannel ch, double t, int pct, int srcVal) {
    if (pct <= 0 || t == 0.0) return 0;
    const double k = (pct / 100.0) * t;
    if (ch == eHue) return (int)std::lround(k * 180.0);
    if (k < 0.0) return (int)std::lround(k * srcVal);
    return (int)std::lround(k * (ChannelMaxValues[ch] - srcVal));
  }

  static void applyDelta(ColorModel *c, ColorChannel ch, int delta) {
    if (!delta) return;
    if (ch == eHue) {
      c->setValue(eHue, wrapHue(c->getValue(eHue) + delta));
      return;
    }
    const int maxv = ChannelMaxValues[ch];
    c->setValue(ch, qBound(0, c->getValue(ch) + delta, maxv));
  }

  void applyGridSize() {
    if (m_dense) {
      m_cols = 15;
      m_rows = 9;
    } else {
      m_cols = 9;
      m_rows = 5;
    }
  }

  void persistAxes() {
    StyleEditorNeighborsHAxis = (int)m_hAxis;
    StyleEditorNeighborsVAxis = (int)m_vAxis;
    StyleEditorNeighborsHPct  = m_hPct;
    StyleEditorNeighborsVPct  = m_vPct;
  }

  void setDense(bool dense) {
    if (m_dense == dense) return;
    m_dense                       = dense;
    StyleEditorNeighborsDenseGrid = dense ? 1 : 0;
    applyGridSize();
    if (m_grid) m_grid->update();
  }

  ColorModel colorAt(int row, int col) const {
    ColorModel c     = m_src;
    const int midC   = m_cols / 2;
    const int midR   = m_rows / 2;
    const double u   = midC ? (double)(col - midC) / (double)midC : 0.0;
    const double v   = midR ? (double)(midR - row) / (double)midR : 0.0;
    applyDelta(&c, m_hAxis,
               axisDelta(m_hAxis, u, m_hPct, m_src.getValue(m_hAxis)));
    applyDelta(&c, m_vAxis,
               axisDelta(m_vAxis, v, m_vPct, m_src.getValue(m_vAxis)));
    return c;
  }

  QToolButton *makeAxisBtn(bool horiz) {
    QToolButton *btn = new QToolButton(this);
    btn->setAutoRaise(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setFixedSize(18, 18);
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setPopupMode(QToolButton::InstantPopup);
    btn->setToolTip(horiz ? tr("Horizontal axis") : tr("Vertical axis"));
    QMenu *menu = new QMenu(btn);
    auto add = [menu](ColorChannel ch, const QString &label) {
      QAction *a = menu->addAction(label);
      a->setData((int)ch);
    };
    add(eHue, tr("H  Hue"));
    add(eSaturation, tr("S  Saturation"));
    add(eValue, tr("V  Value"));
    add(eRed, tr("R  Red"));
    add(eGreen, tr("G  Green"));
    add(eBlue, tr("B  Blue"));
    btn->setMenu(menu);
    connect(menu, &QMenu::triggered, this, [this, horiz, btn](QAction *a) {
      const ColorChannel ch =
          normAxis(a->data().toInt(), horiz ? eHue : eValue);
      if (horiz)
        m_hAxis = ch;
      else
        m_vAxis = ch;
      btn->setText(axisLetter(ch));
      persistAxes();
      if (m_grid) m_grid->update();
    });
    return btn;
  }

  QSlider *makePctSlider(bool horiz) {
    QSlider *s = new QSlider(horiz ? Qt::Horizontal : Qt::Vertical, this);
    s->setRange(0, 100);
    s->setFocusPolicy(Qt::NoFocus);
    s->setToolTip(tr("Range"));
    return s;
  }

  class Grid final : public QWidget {
    ColorNeighborsPane *m_p;
    struct Hit {
      int row = -1;
      int col = -1;
    };
    Hit m_hover;

    QRect chipRect(int row, int col) const {
      const QRect area = contentsRect().adjusted(2, 2, -2, -2);
      if (area.width() <= 0 || area.height() <= 0) return QRect();
      const int cw = (area.width() - kGap * (m_p->m_cols - 1)) / m_p->m_cols;
      const int ch = (area.height() - kGap * (m_p->m_rows - 1)) / m_p->m_rows;
      if (cw <= 0 || ch <= 0) return QRect();
      return QRect(area.x() + col * (cw + kGap), area.y() + row * (ch + kGap),
                   cw, ch);
    }

    Hit hit(const QPoint &p) const {
      for (int row = 0; row < m_p->m_rows; ++row)
        for (int col = 0; col < m_p->m_cols; ++col)
          if (chipRect(row, col).contains(p)) return {row, col};
      return {};
    }

    void drawOutlinedText(QPainter &p, const QRect &r, const QString &s) {
      const int align = Qt::AlignHCenter | Qt::AlignVCenter;
      p.setPen(QColor(0, 0, 0));
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          if (dx || dy) p.drawText(r.translated(dx, dy), align, s);
      p.setPen(QColor(255, 255, 255));
      p.drawText(r, align, s);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
      QPainter p(this);
      p.fillRect(rect(), palette().window());
      const int midR = m_p->m_rows / 2;
      const int midC = m_p->m_cols / 2;
      for (int row = 0; row < m_p->m_rows; ++row) {
        for (int col = 0; col < m_p->m_cols; ++col) {
          const QRect r = chipRect(row, col);
          if (!r.isValid()) continue;
          const TPixel32 pix = m_p->colorAt(row, col).getTPixel();
          p.fillRect(r, QColor(pix.r, pix.g, pix.b, pix.m));
          p.setPen(QPen(palette().mid(), 1));
          p.drawRect(r.adjusted(0, 0, -1, -1));
          if (row == midR && col == midC) {
            p.setPen(QPen(QColor(255, 255, 255), 1));
            p.drawRect(r.adjusted(1, 1, -2, -2));
            p.setPen(QPen(QColor(0, 0, 0), 1));
            p.drawRect(r.adjusted(2, 2, -3, -3));
          }
        }
      }
      if (m_hover.row >= 0) {
        QFont f = font();
        f.setPixelSize(10);
        p.setFont(f);
        const QFontMetrics fm(f);
        const QString title  = tr("Neighbors");
        const QString action = tr("Apply");
        const int w =
            std::max(fm.horizontalAdvance(title), fm.horizontalAdvance(action));
        const int lh   = fm.height();
        const QRect chip = chipRect(m_hover.row, m_hover.col);
        int x            = chip.center().x() - w / 2;
        int y            = chip.bottom() + 4;
        if (y + 2 * lh > height() - 2) y = chip.top() - 2 * lh - 2;
        x = qBound(2, x, std::max(2, width() - w - 2));
        y = qBound(2, y, std::max(2, height() - 2 * lh - 2));
        drawOutlinedText(p, QRect(x, y, w, lh), title);
        drawOutlinedText(p, QRect(x, y + lh, w, lh), action);
      }
    }

    void mousePressEvent(QMouseEvent *e) override {
      const Hit h = hit(e->pos());
      if (h.row < 0 || e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
      }
      const ColorModel c = m_p->colorAt(h.row, h.col);
      if ((e->modifiers() & Qt::ControlModifier) && m_p->m_collect)
        m_p->m_collect(c);
      else if (m_p->m_pick)
        m_p->m_pick(c);
      e->accept();
    }

    void mouseMoveEvent(QMouseEvent *e) override {
      const Hit h = hit(e->pos());
      if (h.row == m_hover.row && h.col == m_hover.col) return;
      m_hover = h;
      update();
    }

    void leaveEvent(QEvent *e) override {
      if (m_hover.row >= 0) {
        m_hover = {};
        update();
      }
      QWidget::leaveEvent(e);
    }

    bool event(QEvent *e) override {
      if (e->type() == QEvent::ToolTip) {
        QToolTip::hideText();
        return true;
      }
      return QWidget::event(e);
    }

    void contextMenuEvent(QContextMenuEvent *e) override {
      QMenu menu(this);
      QAction *compactAct = menu.addAction(tr("Compact grid"));
      compactAct->setCheckable(true);
      compactAct->setChecked(!m_p->m_dense);
      QAction *largeAct = menu.addAction(tr("Large grid"));
      largeAct->setCheckable(true);
      largeAct->setChecked(m_p->m_dense);
      QAction *chosen = menu.exec(e->globalPos());
      if (chosen == compactAct)
        m_p->setDense(false);
      else if (chosen == largeAct)
        m_p->setDense(true);
    }

  public:
    explicit Grid(ColorNeighborsPane *pane) : QWidget(pane), m_p(pane) {
      setMinimumSize(0, 0);
      setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      setMouseTracking(true);
    }
  };

  Grid *m_grid = nullptr;

public:
  explicit ColorNeighborsPane(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_dense = StyleEditorNeighborsDenseGrid != 0;
    m_hAxis = normAxis((int)StyleEditorNeighborsHAxis, eHue);
    m_vAxis = normAxis((int)StyleEditorNeighborsVAxis, eValue);
    m_hPct  = qBound(0, (int)StyleEditorNeighborsHPct, 100);
    m_vPct  = qBound(0, (int)StyleEditorNeighborsVPct, 100);
    applyGridSize();

    m_hSlider = makePctSlider(true);
    m_vSlider = makePctSlider(false);
    m_hLab    = new QLabel(this);
    m_vLab    = new QLabel(this);
    m_hLab->setAlignment(Qt::AlignCenter);
    m_vLab->setAlignment(Qt::AlignCenter);
    QToolButton *hBtn = makeAxisBtn(true);
    QToolButton *vBtn = makeAxisBtn(false);
    hBtn->setText(axisLetter(m_hAxis));
    vBtn->setText(axisLetter(m_vAxis));
    m_hSlider->setValue(m_hPct);
    m_vSlider->setValue(m_vPct);
    m_hLab->setText(QString::number(m_hPct) + QStringLiteral(" %"));
    m_vLab->setText(QString::number(m_vPct) + QStringLiteral(" %"));

    connect(m_hSlider, &QSlider::valueChanged, this, [this](int v) {
      m_hPct = v;
      m_hLab->setText(QString::number(v) + QStringLiteral(" %"));
      persistAxes();
      if (m_grid) m_grid->update();
    });
    connect(m_vSlider, &QSlider::valueChanged, this, [this](int v) {
      m_vPct = v;
      m_vLab->setText(QString::number(v) + QStringLiteral(" %"));
      persistAxes();
      if (m_grid) m_grid->update();
    });

    m_grid = new Grid(this);

    QWidget *topBar = new QWidget(this);
    QHBoxLayout *topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(0, 0, 0, 0);
    topLay->setSpacing(4);
    topLay->addWidget(hBtn, 0);
    topLay->addWidget(m_hSlider, 1);
    topLay->addWidget(m_hLab, 0);

    QWidget *sideBar = new QWidget(this);
    QVBoxLayout *sideLay = new QVBoxLayout(sideBar);
    sideLay->setContentsMargins(0, 0, 0, 0);
    sideLay->setSpacing(2);
    sideLay->addWidget(vBtn, 0, Qt::AlignHCenter);
    sideLay->addWidget(m_vSlider, 1, Qt::AlignHCenter);
    sideLay->addWidget(m_vLab, 0, Qt::AlignHCenter);
    sideBar->setFixedWidth(36);

    QGridLayout *lay = new QGridLayout(this);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);
    lay->addWidget(topBar, 0, 1);
    lay->addWidget(sideBar, 1, 0);
    lay->addWidget(m_grid, 1, 1);
    lay->setRowStretch(1, 1);
    lay->setColumnStretch(1, 1);
  }

  void setFrom(const ColorModel &color) {
    m_src = color;
    if (m_grid) m_grid->update();
  }
  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }
  void setCollect(std::function<void(const ColorModel &)> cb) {
    m_collect = std::move(cb);
  }
};

class ColorBlendPane final : public QWidget {
  static const int kGap = 1;
  ColorModel m_src;
  ColorModel m_c[4];
  bool m_has[4] = {false, false, false, false};
  std::function<void(const ColorModel &)> m_pick;
  std::function<void(const ColorModel &)> m_collect;
  bool m_dense = false;
  int m_cols   = 9;
  int m_rows   = 7;
  struct Hit {
    int row = -1;
    int col = -1;
  };
  Hit m_hover;

  static TEnv::StringVar *blendEnv(int i) {
    if (i == 0) return &StyleEditorColorBlend0;
    if (i == 1) return &StyleEditorColorBlend1;
    if (i == 2) return &StyleEditorColorBlend2;
    return &StyleEditorColorBlend3;
  }

  void applyGridSize() {
    if (m_dense) {
      m_cols = 15;
      m_rows = 11;
    } else {
      m_cols = 9;
      m_rows = 7;
    }
  }

  void setDense(bool dense) {
    if (m_dense == dense) return;
    m_dense                   = dense;
    StyleEditorBlendDenseGrid = dense ? 1 : 0;
    applyGridSize();
    update();
  }

  void resetCorner(int id) {
    m_has[id] = false;
    persist();
    update();
  }

  void resetAllCorners() {
    for (int i = 0; i < 4; ++i) m_has[i] = false;
    persist();
    update();
  }

  QRect chipRect(int row, int col) const {
    const QRect area = contentsRect().adjusted(4, 4, -4, -4);
    if (area.width() <= 0 || area.height() <= 0) return QRect();
    const int cw = (area.width() - kGap * (m_cols - 1)) / m_cols;
    const int ch = (area.height() - kGap * (m_rows - 1)) / m_rows;
    if (cw <= 0 || ch <= 0) return QRect();
    return QRect(area.x() + col * (cw + kGap), area.y() + row * (ch + kGap), cw,
                 ch);
  }

  Hit hit(const QPoint &p) const {
    for (int row = 0; row < m_rows; ++row)
      for (int col = 0; col < m_cols; ++col)
        if (chipRect(row, col).contains(p)) return {row, col};
    return {};
  }

  bool isCorner(int row, int col) const {
    return (row == 0 || row == m_rows - 1) && (col == 0 || col == m_cols - 1);
  }

  static int cornerId(int row, int col) {
    return (row == 0 ? 0 : 2) + (col == 0 ? 0 : 1);
  }

  ColorModel cornerColor(int id) const {
    if (m_has[id]) return m_c[id];
    if (id == 1) {
      ColorModel w;
      w.setTPixel(TPixel32(255, 255, 255, 255));
      return w;
    }
    if (id == 2) {
      ColorModel b;
      b.setTPixel(TPixel32(0, 0, 0, 255));
      return b;
    }
    if (id == 3) return colorAtHue(m_src, wrapHue(m_src.getValue(eHue) + 180));
    return m_src;
  }

  ColorModel colorAt(int row, int col) const {
    const double u = (double)col / (double)(m_cols - 1);
    const double v = (double)row / (double)(m_rows - 1);
    return lerpRgb(lerpRgb(cornerColor(0), cornerColor(1), u),
                   lerpRgb(cornerColor(2), cornerColor(3), u), v);
  }

  void persist() {
    for (int i = 0; i < 4; ++i)
      *blendEnv(i) = m_has[i] ? colorToEnv(m_c[i]) : std::string();
  }

  void restore() {
    for (int i = 0; i < 4; ++i)
      m_has[i] = colorFromEnv((std::string)*blendEnv(i), &m_c[i]);
  }

  void drawOutlinedText(QPainter &p, const QRect &r, const QString &s) {
    const int align = Qt::AlignHCenter | Qt::AlignVCenter;
    p.setPen(QColor(0, 0, 0));
    for (int dx = -1; dx <= 1; ++dx)
      for (int dy = -1; dy <= 1; ++dy)
        if (dx || dy) p.drawText(r.translated(dx, dy), align, s);
    p.setPen(QColor(255, 255, 255));
    p.drawText(r, align, s);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    for (int row = 0; row < m_rows; ++row) {
      for (int col = 0; col < m_cols; ++col) {
        const QRect r = chipRect(row, col);
        if (!r.isValid()) continue;
        const TPixel32 pix = colorAt(row, col).getTPixel();
        p.fillRect(r, QColor(pix.r, pix.g, pix.b, pix.m));
        const bool well = isCorner(row, col);
        p.setPen(QPen(palette().mid(), well ? 2 : 1));
        p.drawRect(r.adjusted(0, 0, -1, -1));
      }
    }
    if (m_hover.row >= 0) {
      QFont f = font();
      f.setPixelSize(10);
      p.setFont(f);
      const QFontMetrics fm(f);
      const QString title  = tr("Intermediate");
      QString action       = tr("Apply");
      if (isCorner(m_hover.row, m_hover.col)) {
        const int id = cornerId(m_hover.row, m_hover.col);
        action       = m_has[id] ? tr("Apply") : tr("Set");
      }
      const int w =
          std::max(fm.horizontalAdvance(title), fm.horizontalAdvance(action));
      const int lh     = fm.height();
      const QRect chip = chipRect(m_hover.row, m_hover.col);
      int x            = chip.center().x() - w / 2;
      int y            = chip.bottom() + 4;
      if (y + 2 * lh > height() - 2) y = chip.top() - 2 * lh - 2;
      x = qBound(2, x, std::max(2, width() - w - 2));
      y = qBound(2, y, std::max(2, height() - 2 * lh - 2));
      drawOutlinedText(p, QRect(x, y, w, lh), title);
      drawOutlinedText(p, QRect(x, y + lh, w, lh), action);
    }
  }

  void mousePressEvent(QMouseEvent *e) override {
    const Hit h = hit(e->pos());
    if (isCorner(h.row, h.col) && (e->modifiers() & Qt::AltModifier)) {
      const int id = cornerId(h.row, h.col);
      if (e->button() == Qt::RightButton) {
        resetCorner(id);
        e->accept();
        return;
      }
      if (e->button() == Qt::LeftButton) {
        m_c[id]   = m_src;
        m_has[id] = true;
        persist();
        update();
        e->accept();
        return;
      }
    }
    if (h.row < 0 || e->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(e);
      return;
    }
    const ColorModel c = colorAt(h.row, h.col);
    if ((e->modifiers() & Qt::ControlModifier) && m_collect)
      m_collect(c);
    else if (m_pick)
      m_pick(c);
    e->accept();
  }

  void mouseMoveEvent(QMouseEvent *e) override {
    const Hit h = hit(e->pos());
    if (h.row == m_hover.row && h.col == m_hover.col) return;
    m_hover = h;
    update();
  }

  void leaveEvent(QEvent *e) override {
    if (m_hover.row >= 0) {
      m_hover = {};
      update();
    }
    QWidget::leaveEvent(e);
  }

  bool event(QEvent *e) override {
    if (e->type() == QEvent::ToolTip) {
      QToolTip::hideText();
      return true;
    }
    return QWidget::event(e);
  }

  void contextMenuEvent(QContextMenuEvent *e) override {
    if (e->modifiers() & Qt::AltModifier) {
      e->accept();
      return;
    }
    const Hit h = hit(e->pos());
    QMenu menu(this);
    QAction *compactAct = menu.addAction(tr("Compact grid"));
    compactAct->setCheckable(true);
    compactAct->setChecked(!m_dense);
    QAction *largeAct = menu.addAction(tr("Large grid"));
    largeAct->setCheckable(true);
    largeAct->setChecked(m_dense);
    menu.addSeparator();
    QAction *resetOne = nullptr;
    if (isCorner(h.row, h.col) && m_has[cornerId(h.row, h.col)])
      resetOne = menu.addAction(tr("Reset this corner"));
    QAction *resetAll = menu.addAction(tr("Reset all corners"));
    QAction *chosen   = menu.exec(e->globalPos());
    if (chosen == compactAct)
      setDense(false);
    else if (chosen == largeAct)
      setDense(true);
    else if (chosen && chosen == resetOne)
      resetCorner(cornerId(h.row, h.col));
    else if (chosen == resetAll)
      resetAllCorners();
  }

public:
  explicit ColorBlendPane(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    m_dense = StyleEditorBlendDenseGrid != 0;
    applyGridSize();
    restore();
  }
  void setFrom(const ColorModel &color) {
    m_src = color;
    update();
  }
  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }
  void setCollect(std::function<void(const ColorModel &)> cb) {
    m_collect = std::move(cb);
  }
};

class ColorMixerPane final : public QWidget {
  static const int kRadius = 14;
  QImage m_img;
  QPixmap m_check;
  QPoint m_last;
  QPoint m_pos;
  QPointF m_view;
  double m_zoom                 = 1.0;
  Qt::MouseButton m_mouseButton = Qt::NoButton;
  bool m_down                   = false;
  bool m_moved                  = false;
  bool m_paint                  = false;
  MixerBlend m_blend            = MixerRyb;
  int m_bgKind                  = 0;
  bool m_space                  = false;
  int m_panKey                  = 0;
  static const int kHistMax     = 24;
  QList<QImage> m_undo;
  QList<QImage> m_redo;
  QImage m_strokeBefore;
  bool m_histOpen               = false;
  QWidget *m_bar                = nullptr;
  QToolButton *m_undoBtn        = nullptr;
  QToolButton *m_redoBtn        = nullptr;
  QToolButton *m_mixModeBtn     = nullptr;
  std::function<ColorModel()> m_current;
  std::function<void(const ColorModel &)> m_pick;

  QPoint toImg(const QPoint &w) const {
    const double z = m_zoom < 0.01 ? 0.01 : m_zoom;
    return QPoint((int)std::floor((w.x() - m_view.x()) / z),
                  (int)std::floor((w.y() - m_view.y()) / z));
  }

  void resetView() {
    m_view = QPointF();
    m_zoom = 1.0;
  }

  void panQt(const QPoint &delta) {
    if (delta.isNull()) return;
    m_view += QPointF(delta);
    update();
  }

  void zoomQt(const QPoint &center, double factor) {
    if (factor == 1.0) return;
    const double old = m_zoom;
    m_zoom           = qBound(0.05, m_zoom * factor, 32.0);
    if (old > 1e-6)
      m_view = QPointF(center) - (QPointF(center) - m_view) * (m_zoom / old);
    update();
  }

  void rebuildCheck(const QSize &s) {
    if (s.width() < 1 || s.height() < 1 || m_check.size() == s) return;
    m_check = QPixmap(s);
    QPainter cp(&m_check);
    const QColor ca = palette().color(QPalette::Mid);
    const QColor cb = palette().color(QPalette::Window);
    const int t     = 8;
    for (int y = 0; y < s.height(); y += t)
      for (int x = 0; x < s.width(); x += t)
        cp.fillRect(x, y, t, t, ((x / t + y / t) & 1) ? ca : cb);
  }

  void ensureImage() {
    const QSize s = size();
    rebuildCheck(s);
    if (s.width() < 8 || s.height() < 8) return;
    if (m_img.isNull()) {
      m_img = QImage(s, QImage::Format_ARGB32);
      m_img.fill(qRgba(0, 0, 0, 0));
      return;
    }
    if (m_img.width() >= s.width() && m_img.height() >= s.height()) return;
    const QSize next(std::max(m_img.width(), s.width()),
                     std::max(m_img.height(), s.height()));
    QImage grown(next, QImage::Format_ARGB32);
    grown.fill(qRgba(0, 0, 0, 0));
    QPainter p(&grown);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(0, 0, m_img);
    m_img = grown;
  }

  void syncHistBtns() {
    if (m_undoBtn) m_undoBtn->setEnabled(!m_undo.isEmpty());
    if (m_redoBtn) m_redoBtn->setEnabled(!m_redo.isEmpty());
  }

  void beginStrokeHist() {
    ensureImage();
    m_strokeBefore = m_img.copy();
    m_histOpen     = true;
  }

  void commitStrokeHist() {
    if (!m_histOpen) return;
    m_histOpen = false;
    if (!m_moved && !m_paint) {
      m_strokeBefore = QImage();
      return;
    }
    m_undo.append(m_strokeBefore);
    m_strokeBefore = QImage();
    while (m_undo.size() > kHistMax) m_undo.removeFirst();
    m_redo.clear();
    syncHistBtns();
  }

  void pushHist() {
    if (m_img.isNull()) return;
    m_undo.append(m_img.copy());
    while (m_undo.size() > kHistMax) m_undo.removeFirst();
    m_redo.clear();
    syncHistBtns();
  }

  void undoHist() {
    if (m_undo.isEmpty()) return;
    m_redo.append(m_img);
    m_img = m_undo.takeLast();
    update();
    syncHistBtns();
  }

  void redoHist() {
    if (m_redo.isEmpty()) return;
    m_undo.append(m_img);
    m_img = m_redo.takeLast();
    update();
    syncHistBtns();
  }

  struct Ryb {
    float r, y, b;
  };

  static Ryb toRyb(int r, int g, int b) {
    float rf = r / 255.f, gf = g / 255.f, bf = b / 255.f;
    const float w = std::min(rf, std::min(gf, bf));
    rf -= w;
    gf -= w;
    bf -= w;
    const float mg = std::max(rf, std::max(gf, bf));
    float y        = std::min(rf, gf);
    rf -= y;
    gf -= y;
    if (bf > 0.f && gf > 0.f) {
      bf *= 0.5f;
      gf *= 0.5f;
    }
    y += gf;
    bf += gf;
    const float my = std::max(rf, std::max(y, bf));
    if (my > 1e-6f && mg > 0.f) {
      const float n = mg / my;
      rf *= n;
      y *= n;
      bf *= n;
    }
    Ryb o;
    o.r = rf + w;
    o.y = y + w;
    o.b = bf + w;
    return o;
  }

  static void fromRyb(const Ryb &o, int &r, int &g, int &b) {
    float rf = o.r, y = o.y, bf = o.b;
    const float w = std::min(rf, std::min(y, bf));
    rf -= w;
    y -= w;
    bf -= w;
    const float my = std::max(rf, std::max(y, bf));
    float gf       = std::min(y, bf);
    y -= gf;
    bf -= gf;
    if (bf > 0.f && gf > 0.f) {
      bf *= 2.f;
      gf *= 2.f;
    }
    rf += y;
    gf += y;
    const float mg = std::max(rf, std::max(gf, bf));
    if (mg > 1e-6f && my > 0.f) {
      const float n = my / mg;
      rf *= n;
      gf *= n;
      bf *= n;
    }
    r = qBound(0, (int)std::lround((rf + w) * 255.f), 255);
    g = qBound(0, (int)std::lround((gf + w) * 255.f), 255);
    b = qBound(0, (int)std::lround((bf + w) * 255.f), 255);
  }

  static void mixRyb(int r1, int g1, int b1, int r2, int g2, int b2, int t,
                     int &ro, int &go, int &bo) {
    t = qBound(0, t, 256);
    if (t <= 0) {
      ro = r1;
      go = g1;
      bo = b1;
      return;
    }
    if (t >= 256) {
      ro = r2;
      go = g2;
      bo = b2;
      return;
    }
    const Ryb a   = toRyb(r1, g1, b1);
    const Ryb b   = toRyb(r2, g2, b2);
    const float u = t / 256.f;
    Ryb o;
    o.r = a.r + (b.r - a.r) * u;
    o.y = a.y + (b.y - a.y) * u;
    o.b = a.b + (b.b - a.b) * u;
    fromRyb(o, ro, go, bo);
  }

  struct OkLab {
    float L, a, b;
  };

  static float srgbToLin(float c) {
    c = qBound(0.f, c, 1.f);
    return c <= 0.04045f ? c / 12.92f
                         : std::pow((c + 0.055f) / 1.055f, 2.4f);
  }

  static float linToSrgb(float c) {
    c = qBound(0.f, c, 1.f);
    return c <= 0.0031308f ? 12.92f * c
                           : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
  }

  static OkLab rgbToOklab(int r, int g, int b) {
    const float rl = srgbToLin(r / 255.f);
    const float gl = srgbToLin(g / 255.f);
    const float bl = srgbToLin(b / 255.f);
    const float l =
        std::cbrt(0.4122214708f * rl + 0.5363325363f * gl + 0.0514459929f * bl);
    const float m =
        std::cbrt(0.2119034982f * rl + 0.6806995451f * gl + 0.1073969566f * bl);
    const float s =
        std::cbrt(0.0883024619f * rl + 0.2817188376f * gl + 0.6299787005f * bl);
    OkLab o;
    o.L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    o.a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    o.b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
    return o;
  }

  static void oklabToLinear(float L, float a, float b, float &r, float &g,
                            float &bl) {
    const float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = L - 0.0894841775f * a - 1.2914855480f * b;
    const float l  = l_ * l_ * l_;
    const float m  = m_ * m_ * m_;
    const float s  = s_ * s_ * s_;
    r              = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    g              = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    bl             = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
  }

  static bool linInGamut(float r, float g, float b) {
    return r >= 0.f && r <= 1.f && g >= 0.f && g <= 1.f && b >= 0.f && b <= 1.f;
  }

  static void oklabToRgb(const OkLab &o, int &r, int &g, int &b) {
    float rl, gl, bl;
    oklabToLinear(o.L, o.a, o.b, rl, gl, bl);
    if (!linInGamut(rl, gl, bl)) {
      float lo = 0.f, hi = 1.f;
      for (int i = 0; i < 10; ++i) {
        const float k = (lo + hi) * 0.5f;
        oklabToLinear(o.L, o.a * k, o.b * k, rl, gl, bl);
        if (linInGamut(rl, gl, bl))
          lo = k;
        else
          hi = k;
      }
      oklabToLinear(o.L, o.a * lo, o.b * lo, rl, gl, bl);
    }
    r = qBound(0, (int)std::lround(linToSrgb(rl) * 255.f), 255);
    g = qBound(0, (int)std::lround(linToSrgb(gl) * 255.f), 255);
    b = qBound(0, (int)std::lround(linToSrgb(bl) * 255.f), 255);
  }

  static float wrapPi(float d) {
    const float pi = 3.14159265f;
    while (d > pi) d -= 2.f * pi;
    while (d < -pi) d += 2.f * pi;
    return d;
  }

  static void mixSoft(int r1, int g1, int b1, int r2, int g2, int b2, int t,
                       int &ro, int &go, int &bo) {
    t = qBound(0, t, 256);
    if (t <= 0) {
      ro = r1;
      go = g1;
      bo = b1;
      return;
    }
    if (t >= 256) {
      ro = r2;
      go = g2;
      bo = b2;
      return;
    }
    const OkLab A = rgbToOklab(r1, g1, b1);
    const OkLab B = rgbToOklab(r2, g2, b2);
    const float u = t / 256.f;
    const float C1  = std::hypot(A.a, A.b);
    const float C2  = std::hypot(B.a, B.b);
    const float h1  = std::atan2(A.b, A.a);
    const float h2  = std::atan2(B.b, B.a);
    const float dH  = wrapPi(h2 - h1);
    const float sep = std::fabs(dH) / 3.14159265f;
    float h;
    if ((A.b * B.b < 0.f) && sep > 0.35f) {
      const float greenH = 2.53f;
      if (u < 0.5f)
        h = h1 + wrapPi(greenH - h1) * (u * 2.f);
      else
        h = greenH + wrapPi(h2 - greenH) * ((u - 0.5f) * 2.f);
    } else {
      h = h1 + dH * u;
    }
    const float C = C1 + (C2 - C1) * u;
    OkLab o;
    o.L = A.L + (B.L - A.L) * u;
    o.a = C * std::cos(h);
    o.b = C * std::sin(h);
    oklabToRgb(o, ro, go, bo);
  }

  static int mixChan(int from, int to, int t) {
    return from + (to - from) * t / 256;
  }

  static void mixRgb(int r1, int g1, int b1, int r2, int g2, int b2, int t,
                     int &ro, int &go, int &bo) {
    t  = qBound(0, t, 256);
    ro = mixChan(r1, r2, t);
    go = mixChan(g1, g2, t);
    bo = mixChan(b1, b2, t);
  }

  void mixColors(int r1, int g1, int b1, int r2, int g2, int b2, int t, int &ro,
                 int &go, int &bo) const {
    if (m_blend == MixerRyb)
      mixRyb(r1, g1, b1, r2, g2, b2, t, ro, go, bo);
    else if (m_blend == MixerSoft)
      mixSoft(r1, g1, b1, r2, g2, b2, t, ro, go, bo);
    else
      mixRgb(r1, g1, b1, r2, g2, b2, t, ro, go, bo);
  }

  static int cover255(int dx, int dy, int radius) {
    const int r2 = radius * radius;
    const int d2 = dx * dx + dy * dy;
    if (d2 >= r2) return 0;
    const int fo = 256 - d2 * 256 / r2;
    return std::min(255, fo * fo / 256);
  }

  void stampPaint(const QPoint &c, QRgb color, int radius) {
    if (m_img.isNull()) return;
    const QRect box = m_img.rect().intersected(
        QRect(c.x() - radius, c.y() - radius, radius * 2 + 1, radius * 2 + 1));
    if (!box.isValid()) return;
    const int cr = qRed(color);
    const int cg = qGreen(color);
    const int cb = qBlue(color);
    for (int y = box.top(); y <= box.bottom(); ++y) {
      QRgb *line   = reinterpret_cast<QRgb *>(m_img.scanLine(y));
      const int dy = y - c.y();
      for (int x = box.left(); x <= box.right(); ++x) {
        const int cv = cover255(x - c.x(), dy, radius);
        if (cv < 8) continue;
        const QRgb dst = line[x];
        const int da   = qAlpha(dst);
        if (da < 8) {
          line[x] = qRgba(cr, cg, cb, cv);
          continue;
        }
        const int keep = da * (255 - cv) / 255;
        const int na   = cv + keep;
        if (na < 8) continue;
        const int rr = (cr * cv + qRed(dst) * keep) / na;
        const int gg = (cg * cv + qGreen(dst) * keep) / na;
        const int bb = (cb * cv + qBlue(dst) * keep) / na;
        line[x] = qRgba(rr, gg, bb, na);
      }
    }
  }

  void smudgeTo(const QPoint &from, const QPoint &to) {
    if (m_img.isNull() || from == to) return;
    const int r  = kRadius;
    const QRect rf(from.x() - r, from.y() - r, r * 2 + 1, r * 2 + 1);
    const QRect rt(to.x() - r, to.y() - r, r * 2 + 1, r * 2 + 1);
    const QRect box = rf.united(rt).intersected(m_img.rect());
    if (!box.isValid()) return;
    const QImage snap = m_img.copy(box);

    double acc0 = 0, acc1 = 0, acc2 = 0, wsum = 0;
    double wL = 0, wA = 0, wB = 0, wW = 0;
    double cL = 0, cA = 0, cB = 0, cW = 0;
    const bool finger         = (m_blend == MixerFinger);
    const QRect fromBox = rf.intersected(m_img.rect());
    if (!finger) {
      for (int y = fromBox.top(); y <= fromBox.bottom(); ++y) {
        const int dy   = y - from.y();
        const int srcY = y - box.y();
        const QRgb *srcLine =
            reinterpret_cast<const QRgb *>(snap.constScanLine(srcY));
        for (int x = fromBox.left(); x <= fromBox.right(); ++x) {
          const int cv = cover255(x - from.x(), dy, r);
          if (cv < 8) continue;
          const QRgb s  = srcLine[x - box.x()];
          const int sa  = qAlpha(s);
          if (sa < 8) continue;
          const double w = (double)cv * sa;
          if (m_blend == MixerRyb) {
            const Ryb o = toRyb(qRed(s), qGreen(s), qBlue(s));
            acc0 += o.r * w;
            acc1 += o.y * w;
            acc2 += o.b * w;
          } else if (m_blend == MixerSoft) {
            const OkLab o = rgbToOklab(qRed(s), qGreen(s), qBlue(s));
            if (o.b >= 0.f) {
              wL += o.L * w;
              wA += o.a * w;
              wB += o.b * w;
              wW += w;
            } else {
              cL += o.L * w;
              cA += o.a * w;
              cB += o.b * w;
              cW += w;
            }
          } else {
            acc0 += qRed(s) * w;
            acc1 += qGreen(s) * w;
            acc2 += qBlue(s) * w;
          }
          wsum += w;
        }
      }
    }
    int pickR = 0, pickG = 0, pickB = 0;
    const bool havePick = !finger && wsum > 0;
    if (havePick) {
      if (m_blend == MixerRyb) {
        Ryb p;
        p.r = (float)(acc0 / wsum);
        p.y = (float)(acc1 / wsum);
        p.b = (float)(acc2 / wsum);
        fromRyb(p, pickR, pickG, pickB);
      } else if (m_blend == MixerSoft) {
        if (wW > 0 && cW > 0) {
          OkLab warm, cool;
          warm.L = (float)(wL / wW);
          warm.a = (float)(wA / wW);
          warm.b = (float)(wB / wW);
          cool.L = (float)(cL / cW);
          cool.a = (float)(cA / cW);
          cool.b = (float)(cB / cW);
          int wr, wg, wb, cr, cg, cb;
          oklabToRgb(warm, wr, wg, wb);
          oklabToRgb(cool, cr, cg, cb);
          mixSoft(wr, wg, wb, cr, cg, cb,
                   (int)(256.0 * cW / (wW + cW)), pickR, pickG, pickB);
        } else {
          OkLab p;
          if (wW > 0) {
            p.L = (float)(wL / wW);
            p.a = (float)(wA / wW);
            p.b = (float)(wB / wW);
          } else {
            p.L = (float)(cL / cW);
            p.a = (float)(cA / cW);
            p.b = (float)(cB / cW);
          }
          oklabToRgb(p, pickR, pickG, pickB);
        }
      } else {
        pickR = (int)(acc0 / wsum);
        pickG = (int)(acc1 / wsum);
        pickB = (int)(acc2 / wsum);
      }
    }

    const QRect write = rt.intersected(m_img.rect());
    for (int y = write.top(); y <= write.bottom(); ++y) {
      QRgb *line     = reinterpret_cast<QRgb *>(m_img.scanLine(y));
      const int dy   = y - to.y();
      const int srcY = from.y() + dy - box.y();
      if (srcY < 0 || srcY >= snap.height()) continue;
      const QRgb *srcLine =
          reinterpret_cast<const QRgb *>(snap.constScanLine(srcY));
      for (int x = write.left(); x <= write.right(); ++x) {
        const int dx   = x - to.x();
        const int mask = cover255(dx, dy, r);
        if (mask < 8) continue;
        const int srcX = from.x() + dx - box.x();
        if (srcX < 0 || srcX >= snap.width()) continue;
        const QRgb src = srcLine[srcX];
        const int sa   = qAlpha(src);
        const QRgb dst = line[x];
        const int da   = qAlpha(dst);
        if (finger) {
          if (sa < 8) continue;
          if (da < 8) {
            const int na = sa * mask / 255;
            if (na < 8) continue;
            line[x] = qRgba(qRed(src), qGreen(src), qBlue(src), na);
            continue;
          }
          const int t = mask;
          line[x]     = qRgba(mixChan(qRed(dst), qRed(src), t),
                              mixChan(qGreen(dst), qGreen(src), t),
                              mixChan(qBlue(dst), qBlue(src), t),
                              da > 200 ? 255
                                       : std::min(255, da + mask * (255 - da) / 255));
          continue;
        }
        if (da < 8) {
          if (sa < 8) continue;
          const int na = sa * mask / 255;
          if (na < 8) continue;
          int rr = qRed(src), gg = qGreen(src), bb = qBlue(src);
          if (havePick) {
            const int t = mask * 90 / 255;
            mixColors(rr, gg, bb, pickR, pickG, pickB, t, rr, gg, bb);
          }
          line[x] = qRgba(rr, gg, bb, na);
          continue;
        }
        const int smearT = mask * 72 / 255;
        const int mixT   = mask * 96 / 255;
        int rr = qRed(dst), gg = qGreen(dst), bb = qBlue(dst);
        if (sa >= 8)
          mixColors(rr, gg, bb, qRed(src), qGreen(src), qBlue(src), smearT, rr,
                    gg, bb);
        if (havePick)
          mixColors(rr, gg, bb, pickR, pickG, pickB, mixT, rr, gg, bb);
        const int na =
            da > 200 ? 255 : std::min(255, da + mask * (255 - da) / 255);
        line[x] = qRgba(rr, gg, bb, na);
      }
    }
  }

  void stroke(const QPoint &a, const QPoint &b) {
    ensureImage();
    const int steps = std::max(1, (a - b).manhattanLength());
    if (m_paint) {
      if (!m_current) return;
      const TPixel32 pix = m_current().getTPixel();
      const QRgb paint   = qRgba(pix.r, pix.g, pix.b, 255);
      for (int i = 0; i <= steps; ++i) {
        const QPoint p(a.x() + (b.x() - a.x()) * i / steps,
                       a.y() + (b.y() - a.y()) * i / steps);
        stampPaint(p, paint, kRadius);
      }
      return;
    }
    QPoint prev = a;
    for (int i = 1; i <= steps; ++i) {
      const QPoint p(a.x() + (b.x() - a.x()) * i / steps,
                     a.y() + (b.y() - a.y()) * i / steps);
      smudgeTo(prev, p);
      prev = p;
    }
  }

  void pickAt(const QPoint &p) {
    ensureImage();
    if (!m_pick || !m_img.rect().contains(p)) return;
    const QRgb pix = m_img.pixel(p);
    if (qAlpha(pix) < 16) return;
    ColorModel c;
    c.setTPixel(TPixel32((UCHAR)qRed(pix), (UCHAR)qGreen(pix),
                         (UCHAR)qBlue(pix), (UCHAR)qAlpha(pix)));
    m_pick(c);
  }

  // Viewer pan (T_HandView / bare Space), taken before the app shortcut.
  static bool isViewerPanShortcut(const QKeyEvent *ke) {
    const std::string keyStr =
        QKeySequence(ke->key() + ke->modifiers()).toString().toStdString();
    QAction *action = CommandManager::instance()->getActionFromShortcut(keyStr);
    if (action)
      return CommandManager::instance()->getIdFromAction(action) == "T_HandView";
    return ke->key() == Qt::Key_Space && ke->modifiers() == Qt::NoModifier;
  }

  void beginPanHold(int key) {
    m_space  = true;
    m_panKey = key;
    setCursor(Qt::OpenHandCursor);
  }

  void endPanHold() {
    m_space  = false;
    m_panKey = 0;
    if (m_mouseButton != Qt::MiddleButton &&
        !(m_mouseButton == Qt::LeftButton && m_down))
      setCursor(Qt::ArrowCursor);
  }

protected:
  bool event(QEvent *e) override {
    if (e->type() == QEvent::ShortcutOverride) {
      QKeyEvent *ke = static_cast<QKeyEvent *>(e);
      if (isViewerPanShortcut(ke)) {
        e->accept();
        return true;
      }
    }
    return QWidget::event(e);
  }

  void enterEvent(QEvent *e) override {
    setFocus(Qt::OtherFocusReason);
    QWidget::enterEvent(e);
  }

  void focusOutEvent(QFocusEvent *e) override {
    endPanHold();
    QWidget::focusOutEvent(e);
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    if (m_bgKind == 1)
      p.fillRect(rect(), Qt::black);
    else if (m_bgKind == 2)
      p.fillRect(rect(), Qt::white);
    else if (m_bgKind == 3)
      p.fillRect(rect(), palette().window());
    else if (!m_check.isNull())
      p.drawPixmap(0, 0, m_check);
    else
      p.fillRect(rect(), palette().window());
    if (!m_img.isNull()) {
      p.save();
      p.translate(m_view);
      p.scale(m_zoom, m_zoom);
      p.drawImage(0, 0, m_img);
      p.restore();
    }
    if (m_bgKind == 3) {
      p.setPen(palette().color(QPalette::Mid));
      const int barH = m_bar ? m_bar->height() : 0;
      p.drawRect(rect().adjusted(0, 0, -1, -1 - barH));
    }
  }

  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    ensureImage();
    if (m_bar) {
      const int h = 16;
      m_bar->setGeometry(0, height() - h, width(), h);
    }
  }

  void mousePressEvent(QMouseEvent *e) override {
    m_mouseButton = e->button();
    m_pos         = e->pos();
    setFocus(Qt::MouseFocusReason);
    if (m_mouseButton == Qt::MiddleButton ||
        (m_mouseButton == Qt::LeftButton && m_space)) {
      setCursor(Qt::ClosedHandCursor);
      e->accept();
      return;
    }
    if (m_mouseButton != Qt::LeftButton) {
      QWidget::mousePressEvent(e);
      return;
    }
    m_down  = true;
    m_moved = false;
    m_paint = (e->modifiers() & Qt::AltModifier);
    m_last  = toImg(e->pos());
    beginStrokeHist();
    if (m_paint && m_current) {
      const TPixel32 pix = m_current().getTPixel();
      stampPaint(m_last, qRgba(pix.r, pix.g, pix.b, 255), kRadius);
      update();
    }
    e->accept();
  }

  void mouseMoveEvent(QMouseEvent *e) override {
    const QPoint cur = e->pos();
    if ((e->buttons() & Qt::MiddleButton) ||
        m_mouseButton == Qt::MiddleButton ||
        (m_space && (e->buttons() & Qt::LeftButton))) {
      panQt(cur - m_pos);
      m_pos = cur;
      e->accept();
      return;
    }
    if (!m_down || m_mouseButton != Qt::LeftButton) return;
    const QPoint p = toImg(cur);
    if (p == m_last) return;
    m_moved = true;
    stroke(m_last, p);
    m_last = p;
    update();
    e->accept();
  }

  void mouseReleaseEvent(QMouseEvent *e) override {
    if (m_mouseButton == Qt::MiddleButton ||
        (e->button() == Qt::LeftButton && m_space)) {
      m_mouseButton = Qt::NoButton;
      m_down        = false;
      setCursor(m_space ? Qt::OpenHandCursor : Qt::ArrowCursor);
      e->accept();
      return;
    }
    m_mouseButton = Qt::NoButton;
    if (e->button() != Qt::LeftButton || !m_down) {
      QWidget::mouseReleaseEvent(e);
      return;
    }
    m_down = false;
    commitStrokeHist();
    if (!m_moved && !m_paint) pickAt(toImg(e->pos()));
    e->accept();
  }

  void keyPressEvent(QKeyEvent *e) override {
    if (isViewerPanShortcut(e)) {
      beginPanHold(e->key());
      e->accept();
      return;
    }
    const int key = e->key();
    if (key == '+' || key == Qt::Key_Plus || key == Qt::Key_Equal) {
      zoomQt(rect().center(), exp(0.001 * 120));
      e->accept();
      return;
    }
    if (key == '-' || key == Qt::Key_Minus) {
      zoomQt(rect().center(), exp(0.001 * -120));
      e->accept();
      return;
    }
    if (key == '0' || key == Qt::Key_0) {
      resetView();
      update();
      e->accept();
      return;
    }
    QWidget::keyPressEvent(e);
  }

  void keyReleaseEvent(QKeyEvent *e) override {
    if (m_space && (e->key() == m_panKey || isViewerPanShortcut(e))) {
      endPanHold();
      e->accept();
      return;
    }
    QWidget::keyReleaseEvent(e);
  }

  void wheelEvent(QWheelEvent *e) override {
    int delta = 0;
    switch (e->source()) {
    case Qt::MouseEventNotSynthesized:
      delta = (e->modifiers() & Qt::AltModifier) ? e->angleDelta().x()
                                                 : e->angleDelta().y();
      break;
    case Qt::MouseEventSynthesizedBySystem:
      if (!e->pixelDelta().isNull())
        delta = e->pixelDelta().y();
      else if (!e->angleDelta().isNull())
        delta = (e->angleDelta() / 8 / 15).y();
      break;
    default:
      break;
    }
    if (delta != 0) {
      const int d = delta > 0 ? 120 : -120;
      const QPoint center =
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
          e->position().toPoint();
#else
          e->pos();
#endif
      zoomQt(center, std::exp(0.001 * d));
    }
    e->accept();
  }

public:
  explicit ColorMixerPane(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(tr("Color Mixer"));

    m_bar = new QWidget(this);
    m_bar->setFixedHeight(16);
    m_bar->setAutoFillBackground(true);
    m_bar->setFocusPolicy(Qt::NoFocus);
    auto *barLay = new QHBoxLayout(m_bar);
    barLay->setContentsMargins(2, 0, 2, 0);
    barLay->setSpacing(0);

    auto mkBtn = [this](const char *icon, const QString &tip) {
      QToolButton *b = new QToolButton(m_bar);
      b->setAutoRaise(true);
      b->setFocusPolicy(Qt::NoFocus);
      b->setFixedSize(16, 16);
      b->setIconSize(QSize(11, 11));
      b->setToolButtonStyle(Qt::ToolButtonIconOnly);
      if (icon) b->setIcon(createQIcon(icon));
      b->setToolTip(tip);
      return b;
    };

    barLay->addStretch(1);

    m_undoBtn = mkBtn("undo", tr("Undo"));
    m_undoBtn->setEnabled(false);
    connect(m_undoBtn, &QToolButton::clicked, this, [this]() { undoHist(); });
    barLay->addWidget(m_undoBtn);

    m_redoBtn = mkBtn("redo", tr("Redo"));
    m_redoBtn->setEnabled(false);
    connect(m_redoBtn, &QToolButton::clicked, this, [this]() { redoHist(); });
    barLay->addWidget(m_redoBtn);

    QToolButton *clearBtn = mkBtn("clear", tr("Clear"));
    connect(clearBtn, &QToolButton::clicked, this, [this]() {
      if (m_img.isNull()) return;
      pushHist();
      m_img.fill(qRgba(0, 0, 0, 0));
      resetView();
      update();
    });
    barLay->addWidget(clearBtn);

    m_blend      = normalizedMixerBlend(StyleEditorMixerPaintMix);
    m_mixModeBtn = mkBtn(0, QString());
    m_mixModeBtn->setCheckable(true);
    auto syncMixMode = [this]() {
      const char *icon = "colorpicker_mixer_pigment";
      QString tip      = tr("RYB");
      if (m_blend == MixerRgb) {
        icon = "colorpicker_mixer_rgb";
        tip  = tr("RGB");
      } else if (m_blend == MixerSoft) {
        icon = "paintbrush";
        tip  = tr("Soft");
      } else if (m_blend == MixerFinger) {
        icon = "finger";
        tip  = tr("Finger");
      }
      m_mixModeBtn->setIcon(createQIcon(icon));
      m_mixModeBtn->setToolTip(tip);
      m_mixModeBtn->setChecked(m_blend == MixerRgb);
    };
    syncMixMode();
    connect(m_mixModeBtn, &QToolButton::clicked, this, [this, syncMixMode]() {
      if (m_blend == MixerRyb)
        m_blend = MixerRgb;
      else if (m_blend == MixerRgb)
        m_blend = MixerSoft;
      else if (m_blend == MixerSoft)
        m_blend = MixerFinger;
      else
        m_blend = MixerRyb;
      StyleEditorMixerPaintMix = (int)m_blend;
      syncMixMode();
    });
    barLay->addWidget(m_mixModeBtn);

    auto *gap = new QWidget(m_bar);
    gap->setFixedWidth(4);
    barLay->addWidget(gap);

    m_bgKind               = qBound(0, (int)StyleEditorMixerBg, 3);
    QToolButton *bgChecker =
        mkBtn("browser_preview_checkboard", tr("Checkered background"));
    QToolButton *bgBlack =
        mkBtn("browser_preview_black", tr("Black background"));
    QToolButton *bgWhite =
        mkBtn("browser_preview_white", tr("White background"));
    QToolButton *bgNone =
        mkBtn("browser_preview_transparency", tr("Transparent background"));
    bgChecker->setCheckable(true);
    bgBlack->setCheckable(true);
    bgWhite->setCheckable(true);
    bgNone->setCheckable(true);
    auto *bgGroup = new QButtonGroup(this);
    bgGroup->setExclusive(true);
    bgGroup->addButton(bgChecker, 0);
    bgGroup->addButton(bgBlack, 1);
    bgGroup->addButton(bgWhite, 2);
    bgGroup->addButton(bgNone, 3);
    if (QAbstractButton *cur = bgGroup->button(m_bgKind)) cur->setChecked(true);
    auto applyBg = [this](int id) {
      m_bgKind           = id;
      StyleEditorMixerBg = id;
      update();
    };
    connect(bgChecker, &QToolButton::clicked, this, [applyBg]() { applyBg(0); });
    connect(bgBlack, &QToolButton::clicked, this, [applyBg]() { applyBg(1); });
    connect(bgWhite, &QToolButton::clicked, this, [applyBg]() { applyBg(2); });
    connect(bgNone, &QToolButton::clicked, this, [applyBg]() { applyBg(3); });
    barLay->addWidget(bgChecker);
    barLay->addWidget(bgBlack);
    barLay->addWidget(bgWhite);
    barLay->addWidget(bgNone);
  }

  void setCurrent(std::function<ColorModel()> cb) { m_current = std::move(cb); }
  void setPick(std::function<void(const ColorModel &)> cb) {
    m_pick = std::move(cb);
  }
};

class SectionToggleBar final : public QWidget {
  static const int kGap     = 1;
  static const int kRowH    = 20;
  static const int kMaxChip = 36;
  QList<QToolButton *> m_btns;
  bool m_fillRow = false;

public:
  void relayout() {
    QList<QToolButton *> vis;
    for (QToolButton *btn : m_btns) {
      if (btn->isVisibleTo(this)) vis.append(btn);
    }
    const int n = vis.size();
    if (n == 0) return;

    QList<QToolButton *> textBtns;
    QList<QToolButton *> iconBtns;
    for (QToolButton *btn : vis) {
      if (btn->toolButtonStyle() == Qt::ToolButtonIconOnly)
        iconBtns.append(btn);
      else
        textBtns.append(btn);
    }

    const int w  = std::max(0, width());
    const int nT = textBtns.size();
    const int nI = iconBtns.size();

    auto setIconSize = [&](QToolButton *btn, int bw, bool compact) {
      int icon = std::max(8, std::min(bw, kRowH) - 2);
      if (compact) {
        if (btn->property("kind").isValid()) {
          const int kind = btn->property("kind").toInt();
          const int maxKind =
              (kind == (int)AdvancedPickerKind::Wheel) ? 13 : 12;
          icon = std::max(8, std::min(maxKind, bw - 8));
        } else {
          icon = std::max(8, std::min(14, bw - 6));
        }
      }
      btn->setIconSize(QSize(icon, icon));
    };

    auto layoutFillRow = [&]() {
      const int equalW = std::max(0, (w - kGap * (n - 1)) / n);
      const int extra  = std::max(0, w - equalW * n - kGap * (n - 1));
      int fontPx       = (equalW >= 28) ? 12 : 10;
      QFont f          = font();
      for (; fontPx >= 6; --fontPx) {
        f.setPixelSize(fontPx);
        QFontMetrics fm(f);
        bool fits = true;
        for (QToolButton *btn : textBtns) {
          if (fm.horizontalAdvance(btn->text()) + 4 > equalW) {
            fits = false;
            break;
          }
        }
        if (fits) break;
      }
      f.setPixelSize(fontPx);
      const bool compact = equalW < 28;
      int x              = 0;
      for (int i = 0; i < n; ++i) {
        const int bw = equalW + (i < extra ? 1 : 0);
        vis[i]->setFont(f);
        setIconSize(vis[i], bw, compact);
        vis[i]->setGeometry(x, 0, bw, kRowH);
        x += bw + kGap;
      }
    };

    if (m_fillRow) {
      layoutFillRow();
    } else {
      auto textWidthsAt = [&](int fontPx) {
        QFont tf = font();
        tf.setPixelSize(fontPx);
        QFontMetrics fm(tf);
        QList<int> ws;
        for (QToolButton *btn : textBtns) {
          const int tw = std::min(
              kMaxChip, std::max(18, fm.horizontalAdvance(btn->text()) + 8));
          ws.append(tw);
        }
        return ws;
      };
      auto sumGap = [&](const QList<int> &ws) {
        int s = 0;
        for (int x : ws) s += x;
        if (ws.size() > 1) s += kGap * (ws.size() - 1);
        return s;
      };

      int fontPx     = 12;
      QList<int> tWs = textWidthsAt(fontPx);
      int sumT       = sumGap(tWs);
      const int iconChip = kRowH;
      int sumI           = nI * iconChip + kGap * std::max(0, nI - 1);
      const int mid      = (nT > 0 && nI > 0) ? 8 : 0;

      while (fontPx > 10 && sumT + mid + sumI > w) {
        --fontPx;
        tWs  = textWidthsAt(fontPx);
        sumT = sumGap(tWs);
      }

      QFont f = font();
      f.setPixelSize(fontPx);
      if (sumT + mid + sumI <= w) {
        int x = 0;
        for (int i = 0; i < nT; ++i) {
          textBtns[i]->setFont(f);
          textBtns[i]->setGeometry(x, 0, tWs[i], kRowH);
          x += tWs[i] + kGap;
        }
        x = w - sumI;
        for (int i = 0; i < nI; ++i) {
          iconBtns[i]->setFont(f);
          setIconSize(iconBtns[i], iconChip, false);
          iconBtns[i]->setGeometry(x, 0, iconChip, kRowH);
          x += iconChip + kGap;
        }
      } else {
        layoutFillRow();
      }
    }
    if (QWidget *p = parentWidget()) p->setFixedHeight(kRowH);
  }

protected:
  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    relayout();
  }

public:
  explicit SectionToggleBar(QWidget *parent) : QWidget(parent) {
    setMinimumSize(0, kRowH);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  }

  QSize sizeHint() const override { return QSize(0, kRowH); }
  QSize minimumSizeHint() const override { return QSize(0, kRowH); }

  void addButton(QToolButton *btn) {
    m_btns.append(btn);
    relayout();
  }

  void setFillRow(bool on) { m_fillRow = on; }
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

    m_colorFeaturesBar = new QWidget(this);
    m_colorFeaturesBar->setFixedHeight(22);
    m_colorFeaturesBar->setMinimumWidth(0);
    QHBoxLayout *colorFeaturesLay = new QHBoxLayout(m_colorFeaturesBar);
    colorFeaturesLay->setContentsMargins(2, 1, 2, 1);
    colorFeaturesLay->setSpacing(2);

    m_collectorBtn = new QToolButton(m_colorFeaturesBar);
    m_collectorBtn->setCheckable(true);
    m_collectorBtn->setAutoRaise(true);
    m_collectorBtn->setFocusPolicy(Qt::NoFocus);
    m_collectorBtn->setFixedSize(20, 20);
    m_collectorBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_collectorBtn->setIconSize(QSize(18, 18));
    m_collectorBtn->setIcon(createQIcon("colorpicker_collector"));
    m_collectorBtn->setToolTip(tr("Color Collector"));
    colorFeaturesLay->addWidget(m_collectorBtn, 0);

    m_historyBtn = new QToolButton(m_colorFeaturesBar);
    m_historyBtn->setCheckable(true);
    m_historyBtn->setAutoRaise(true);
    m_historyBtn->setFocusPolicy(Qt::NoFocus);
    m_historyBtn->setFixedSize(20, 20);
    m_historyBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_historyBtn->setIconSize(QSize(20, 20));
    m_historyBtn->setIcon(createQIcon("colorpicker_history"));
    m_historyBtn->setToolTip(tr("Color History"));
    colorFeaturesLay->addWidget(m_historyBtn, 0);

    m_harmonyBtn = new QToolButton(m_colorFeaturesBar);
    m_harmonyBtn->setCheckable(true);
    m_harmonyBtn->setAutoRaise(true);
    m_harmonyBtn->setFocusPolicy(Qt::NoFocus);
    m_harmonyBtn->setFixedSize(20, 20);
    m_harmonyBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_harmonyBtn->setIconSize(QSize(20, 20));
    m_harmonyBtn->setIcon(createQIcon("colorpicker_harmony"));
    m_harmonyBtn->setToolTip(tr("Color Harmonies"));
    colorFeaturesLay->addWidget(m_harmonyBtn, 0);

    m_shadesBtn = new QToolButton(m_colorFeaturesBar);
    m_shadesBtn->setCheckable(true);
    m_shadesBtn->setAutoRaise(true);
    m_shadesBtn->setFocusPolicy(Qt::NoFocus);
    m_shadesBtn->setFixedSize(20, 20);
    m_shadesBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_shadesBtn->setIconSize(QSize(16, 16));
    m_shadesBtn->setIcon(createQIcon("colorpicker_shades"));
    m_shadesBtn->setToolTip(tr("Color Shades"));
    colorFeaturesLay->addWidget(m_shadesBtn, 0);

    m_neighborsBtn = new QToolButton(m_colorFeaturesBar);
    m_neighborsBtn->setCheckable(true);
    m_neighborsBtn->setAutoRaise(true);
    m_neighborsBtn->setFocusPolicy(Qt::NoFocus);
    m_neighborsBtn->setFixedSize(20, 20);
    m_neighborsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_neighborsBtn->setIconSize(QSize(16, 16));
    m_neighborsBtn->setIcon(createQIcon("colorpicker_neighbors"));
    m_neighborsBtn->setToolTip(tr("Neighboring Colors"));
    colorFeaturesLay->addWidget(m_neighborsBtn, 0);

    m_blendBtn = new QToolButton(m_colorFeaturesBar);
    m_blendBtn->setCheckable(true);
    m_blendBtn->setAutoRaise(true);
    m_blendBtn->setFocusPolicy(Qt::NoFocus);
    m_blendBtn->setFixedSize(20, 20);
    m_blendBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_blendBtn->setIconSize(QSize(16, 16));
    m_blendBtn->setIcon(createQIcon("colorpicker_blend"));
    m_blendBtn->setToolTip(tr("Intermediate Colors"));
    colorFeaturesLay->addWidget(m_blendBtn, 0);

    m_mixerBtn = new QToolButton(m_colorFeaturesBar);
    m_mixerBtn->setCheckable(true);
    m_mixerBtn->setAutoRaise(true);
    m_mixerBtn->setFocusPolicy(Qt::NoFocus);
    m_mixerBtn->setFixedSize(20, 20);
    m_mixerBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_mixerBtn->setIconSize(QSize(16, 16));
    m_mixerBtn->setIcon(createQIcon("colorpicker_mixer"));
    m_mixerBtn->setToolTip(tr("Color Mixer"));
    colorFeaturesLay->addWidget(m_mixerBtn, 0);
    colorFeaturesLay->addStretch(1);

    QFrame *collectorFrame = new QFrame(this);
    collectorFrame->setObjectName("PlainColorPageParts");
    collectorFrame->setMinimumWidth(0);
    QVBoxLayout *collectorLay = new QVBoxLayout(collectorFrame);
    collectorLay->setContentsMargins(4, 4, 4, 4);
    collectorLay->setSpacing(0);
    m_collectorGrid = new ColorCollectorGrid(collectorFrame);
    collectorLay->addWidget(m_collectorGrid, 1);
    static_cast<ColorCollectorGrid *>(m_collectorGrid)
        ->setCurrent([this]() { return m_color; });
    static_cast<ColorCollectorGrid *>(m_collectorGrid)
        ->setPick([this](const ColorModel &c) {
          if (!(m_color == c)) {
            m_color = c;
            updateControls();
          }
          if (m_signalEnabled) emit colorChanged(m_color, false);
          if (m_historyGrid)
            static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
        });

    QFrame *historyFrame = new QFrame(this);
    historyFrame->setObjectName("PlainColorPageParts");
    historyFrame->setMinimumWidth(0);
    QVBoxLayout *historyLay = new QVBoxLayout(historyFrame);
    historyLay->setContentsMargins(4, 4, 4, 4);
    historyLay->setSpacing(0);
    m_historyGrid = new ColorHistoryGrid(historyFrame);
    historyLay->addWidget(m_historyGrid, 1);
    static_cast<ColorHistoryGrid *>(m_historyGrid)
        ->setPick([this](const ColorModel &c) {
          if (!(m_color == c)) {
            m_color = c;
            updateControls();
          }
          if (m_signalEnabled) emit colorChanged(m_color, false);
        });

    QFrame *harmonyFrame = new QFrame(this);
    harmonyFrame->setObjectName("PlainColorPageParts");
    harmonyFrame->setMinimumWidth(0);
    QVBoxLayout *harmonyLay = new QVBoxLayout(harmonyFrame);
    harmonyLay->setContentsMargins(0, 0, 0, 0);
    harmonyLay->setSpacing(0);
    m_harmonyPane = new ColorHarmonyPane(harmonyFrame);
    harmonyLay->addWidget(m_harmonyPane, 1);
    static_cast<ColorHarmonyPane *>(m_harmonyPane)
        ->setPick([this](const ColorModel &c) {
          if (!(m_color == c)) {
            m_color = c;
            updateControls();
          }
          if (m_signalEnabled) emit colorChanged(m_color, false);
          if (m_historyGrid)
            static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
        });
    static_cast<ColorHarmonyPane *>(m_harmonyPane)->setOnCut([this]() {
      if (m_hexagonalColorWheel) m_hexagonalColorWheel->update();
      if (m_squaredColorWheel) m_squaredColorWheel->update();
      if (m_verticalSlider) m_verticalSlider->update();
      for (int i = 0; i < 7; ++i)
        if (m_channelControls[i]) m_channelControls[i]->update();
    });

    QFrame *shadesFrame = new QFrame(this);
    shadesFrame->setObjectName("PlainColorPageParts");
    shadesFrame->setMinimumWidth(0);
    QVBoxLayout *shadesLay = new QVBoxLayout(shadesFrame);
    shadesLay->setContentsMargins(0, 0, 0, 0);
    shadesLay->setSpacing(0);
    m_shadesPane = new ColorShadesPane(shadesFrame);
    shadesLay->addWidget(m_shadesPane, 1);
    auto applyPickedColor = [this](const ColorModel &c) {
      if (!(m_color == c)) {
        m_color = c;
        updateControls();
      }
      if (m_signalEnabled) emit colorChanged(m_color, false);
      if (m_historyGrid)
        static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
    };
    auto collectColor = [this](const ColorModel &c) {
      if (m_collectorGrid)
        static_cast<ColorCollectorGrid *>(m_collectorGrid)->append(c);
    };
    static_cast<ColorShadesPane *>(m_shadesPane)->setPick(applyPickedColor);
    static_cast<ColorShadesPane *>(m_shadesPane)->setCollect(collectColor);

    QFrame *neighborsFrame = new QFrame(this);
    neighborsFrame->setObjectName("PlainColorPageParts");
    neighborsFrame->setMinimumWidth(0);
    QVBoxLayout *neighborsLay = new QVBoxLayout(neighborsFrame);
    neighborsLay->setContentsMargins(0, 0, 0, 0);
    neighborsLay->setSpacing(0);
    m_neighborsPane = new ColorNeighborsPane(neighborsFrame);
    neighborsLay->addWidget(m_neighborsPane, 1);
    static_cast<ColorNeighborsPane *>(m_neighborsPane)->setPick(applyPickedColor);
    static_cast<ColorNeighborsPane *>(m_neighborsPane)->setCollect(collectColor);

    QFrame *blendFrame = new QFrame(this);
    blendFrame->setObjectName("PlainColorPageParts");
    blendFrame->setMinimumWidth(0);
    QVBoxLayout *blendLay = new QVBoxLayout(blendFrame);
    blendLay->setContentsMargins(0, 0, 0, 0);
    blendLay->setSpacing(0);
    m_blendPane = new ColorBlendPane(blendFrame);
    blendLay->addWidget(m_blendPane, 1);
    static_cast<ColorBlendPane *>(m_blendPane)->setPick(applyPickedColor);
    static_cast<ColorBlendPane *>(m_blendPane)->setCollect(collectColor);

    QFrame *mixerFrame = new QFrame(this);
    mixerFrame->setObjectName("PlainColorPageParts");
    mixerFrame->setMinimumWidth(0);
    QVBoxLayout *mixerLay = new QVBoxLayout(mixerFrame);
    mixerLay->setContentsMargins(0, 0, 0, 0);
    mixerLay->setSpacing(0);
    m_mixerPane = new ColorMixerPane(mixerFrame);
    mixerLay->addWidget(m_mixerPane, 1);
    static_cast<ColorMixerPane *>(m_mixerPane)
        ->setCurrent([this]() { return m_color; });
    static_cast<ColorMixerPane *>(m_mixerPane)
        ->setPick([this](const ColorModel &c) {
          if (!(m_color == c)) {
            m_color = c;
            updateControls();
          }
          if (m_signalEnabled) emit colorChanged(m_color, false);
          if (m_historyGrid)
            static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
        });

    m_featureStack = new QStackedWidget(this);
    m_featureStack->addWidget(m_slidersContainer);
    m_featureStack->addWidget(collectorFrame);
    m_featureStack->addWidget(historyFrame);
    m_featureStack->addWidget(harmonyFrame);
    m_featureStack->addWidget(shadesFrame);
    m_featureStack->addWidget(neighborsFrame);
    m_featureStack->addWidget(blendFrame);
    m_featureStack->addWidget(mixerFrame);
    m_featureStack->setCurrentIndex(0);
    auto uncheckBtn = [](QToolButton *btn) {
      if (!btn || !btn->isChecked()) return;
      bool blocked = btn->blockSignals(true);
      btn->setChecked(false);
      btn->blockSignals(blocked);
    };
    auto uncheckOthers = [this, uncheckBtn](QToolButton *keep) {
      if (keep != m_collectorBtn) uncheckBtn(m_collectorBtn);
      if (keep != m_historyBtn) uncheckBtn(m_historyBtn);
      if (keep != m_harmonyBtn) uncheckBtn(m_harmonyBtn);
      if (keep != m_shadesBtn) uncheckBtn(m_shadesBtn);
      if (keep != m_neighborsBtn) uncheckBtn(m_neighborsBtn);
      if (keep != m_blendBtn) uncheckBtn(m_blendBtn);
      if (keep != m_mixerBtn) uncheckBtn(m_mixerBtn);
    };
    connect(m_collectorBtn, &QToolButton::toggled, this,
            [this, uncheckOthers](bool on) {
              if (on) uncheckOthers(m_collectorBtn);
              syncFeaturePage();
            });
    connect(m_historyBtn, &QToolButton::toggled, this,
            [this, uncheckOthers](bool on) {
              if (on) uncheckOthers(m_historyBtn);
              syncFeaturePage();
            });
    connect(m_harmonyBtn, &QToolButton::toggled, this,
            [this, uncheckOthers](bool on) {
              if (on) uncheckOthers(m_harmonyBtn);
              syncFeaturePage();
            });
    connect(m_shadesBtn, &QToolButton::toggled, this,
            [this, uncheckOthers](bool on) {
              if (on) uncheckOthers(m_shadesBtn);
              syncFeaturePage();
            });
    connect(m_neighborsBtn, &QToolButton::toggled, this,
            [this, uncheckOthers](bool on) {
              if (on) uncheckOthers(m_neighborsBtn);
              syncFeaturePage();
            });
    connect(m_blendBtn, &QToolButton::toggled, this,
            [this, uncheckOthers](bool on) {
              if (on) uncheckOthers(m_blendBtn);
              syncFeaturePage();
            });
    connect(m_mixerBtn, &QToolButton::toggled, this,
            [this, uncheckOthers](bool on) {
              if (on) uncheckOthers(m_mixerBtn);
              syncFeaturePage();
            });

    QWidget *featureHost = new QWidget(this);
    QVBoxLayout *hostLay = new QVBoxLayout(featureHost);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->setSpacing(0);
    hostLay->addWidget(m_colorFeaturesBar, 0);
    hostLay->addWidget(m_featureStack, 1);
    m_vSplitter->addWidget(featureHost);

    m_pickerChrome = new QWidget(this);
    m_pickerChrome->setFixedHeight(20);
    m_pickerChrome->setMinimumWidth(0);
    m_pickerChrome->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout *chromeLay = new QHBoxLayout(m_pickerChrome);
    chromeLay->setContentsMargins(4, 0, 4, 0);
    chromeLay->setSpacing(2);
    const QString chromeIconQss =
        QStringLiteral("QToolButton { margin: 0px; padding: 0px; }");

    m_sectionBar = new SectionToggleBar(m_pickerChrome);
    m_sectionBar->setMinimumWidth(0);
    m_sectionBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_advancedModeBtn = new QToolButton(m_sectionBar);
    m_advancedModeBtn->setCheckable(true);
    m_advancedModeBtn->setMinimumSize(0, 0);
    m_advancedModeBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_advancedModeBtn->setAutoRaise(true);
    m_advancedModeBtn->setFocusPolicy(Qt::NoFocus);
    m_advancedModeBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_advancedModeBtn->setIconSize(QSize(16, 16));
    m_advancedModeBtn->setStyleSheet(chromeIconQss);
    m_advancedModeBtn->setIcon(createQIcon("colorpicker_advanced"));
    connect(m_advancedModeBtn, SIGNAL(clicked()), this,
            SIGNAL(colorPageModeClicked()));

    m_wheelKindBtn = new QToolButton(m_sectionBar);
    m_wheelKindBtn->setCheckable(true);
    m_wheelKindBtn->setMinimumSize(0, 0);
    m_wheelKindBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
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

    m_rectKindBtn = new QToolButton(m_sectionBar);
    m_rectKindBtn->setCheckable(true);
    m_rectKindBtn->setMinimumSize(0, 0);
    m_rectKindBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
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

    chromeLay->addWidget(m_sectionBar, 1);

    mainLayout->addWidget(m_pickerChrome, 0);

    m_variationStrip = new ColorVariationStrip(m_swatchFrame);
    static_cast<ColorVariationStrip *>(m_variationStrip)
        ->setPick([this](const ColorModel &c) {
          if (!(m_color == c)) {
            m_color = c;
            updateControls();
          }
          if (m_signalEnabled) emit colorChanged(m_color, false);
          if (m_historyGrid)
            static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
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
  enableCtx(m_colorFeaturesBar);
  enableCtx(m_slidersContainer);
  if (m_collectorGrid) enableCtx(m_collectorGrid);
  if (m_historyGrid) enableCtx(m_historyGrid);
  if (m_harmonyPane) enableCtx(m_harmonyPane);
  if (m_shadesPane) enableCtx(m_shadesPane);
  if (m_mixerPane) enableCtx(m_mixerPane);
  enableCtx(m_hsvFrame);
  enableCtx(m_alphaFrame);
  enableCtx(m_rgbFrame);
  enableCtx(m_vSplitter);
  enableCtx(m_pickerChrome);
  enableCtx(m_sectionBar);
  enableCtx(m_swatchFrame);

  m_squaredColorWheel->setChannel(eHue);
  updatePickerChrome();
}

//-----------------------------------------------------------------------------

void PlainColorPage::resizeEvent(QResizeEvent *) { placeSvShapeButton(); }

//-----------------------------------------------------------------------------

void PlainColorPage::showEvent(QShowEvent *e) {
  StyleEditorPage::showEvent(e);
  placeSvShapeButton();
  QTimer::singleShot(0, this, SLOT(refreshPickerLayout()));
}

//-----------------------------------------------------------------------------

bool PlainColorPage::eventFilter(QObject *watched, QEvent *event) {
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
  if (m_harmonyPane)
    static_cast<ColorHarmonyPane *>(m_harmonyPane)->setFrom(m_color);
  if (m_shadesPane)
    static_cast<ColorShadesPane *>(m_shadesPane)->setFrom(m_color);
  if (m_neighborsPane)
    static_cast<ColorNeighborsPane *>(m_neighborsPane)->setFrom(m_color);
  if (m_blendPane)
    static_cast<ColorBlendPane *>(m_blendPane)->setFrom(m_color);
}

//-----------------------------------------------------------------------------

void PlainColorPage::syncFeaturePage() {
  if (!m_featureStack) return;
  if (m_mixerBtn && m_mixerBtn->isChecked())
    m_featureStack->setCurrentIndex(7);
  else if (m_blendBtn && m_blendBtn->isChecked())
    m_featureStack->setCurrentIndex(6);
  else if (m_neighborsBtn && m_neighborsBtn->isChecked())
    m_featureStack->setCurrentIndex(5);
  else if (m_shadesBtn && m_shadesBtn->isChecked())
    m_featureStack->setCurrentIndex(4);
  else if (m_harmonyBtn && m_harmonyBtn->isChecked())
    m_featureStack->setCurrentIndex(3);
  else if (m_historyBtn && m_historyBtn->isChecked())
    m_featureStack->setCurrentIndex(2);
  else if (m_collectorBtn && m_collectorBtn->isChecked())
    m_featureStack->setCurrentIndex(1);
  else
    m_featureStack->setCurrentIndex(0);
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
    else if (btn == m_wheelKindBtn || btn == m_rectKindBtn)
      btn->setVisible(showKindBtns);
    else if (btn == m_advancedModeBtn)
      btn->setVisible(showAdvBtn);
    else
      btn->setVisible(showSections);
  }
  const bool showTopChrome =
      showAdvBtn || showSections || showKindBtns || showVarBtn;
  m_sectionBar->setVisible(showTopChrome);
  if (SectionToggleBar *bar =
          static_cast<SectionToggleBar *>(m_sectionBar)) {
    bar->setFillRow(showSections);
    bar->relayout();
  }
  m_svShapeBtn->setVisible(showShapeBtn);
  if (m_hexagonalColorWheel->svShape() == AdvancedSvShape::Square) {
    m_svShapeBtn->setIcon(createQIcon("colorpicker_sv_triangle"));
    m_svShapeBtn->setToolTip(tr("Switch to the triangular chromatic space"));
  } else {
    m_svShapeBtn->setIcon(createQIcon("colorpicker_sv_square"));
    m_svShapeBtn->setToolTip(tr("Switch to the square chromatic space"));
  }
  m_pickerChrome->setVisible(showTopChrome);
  const bool showColorFeaturesBar = StyleEditorShowColorFeaturesBar != 0;
  const bool showCollectorBtn =
      showColorFeaturesBar && StyleEditorShowCollectorButton != 0;
  const bool showHistoryBtn =
      showColorFeaturesBar && StyleEditorShowHistoryButton != 0;
  const bool showHarmonyBtn =
      showColorFeaturesBar && StyleEditorShowHarmonyButton != 0;
  const bool showShadesBtn =
      showColorFeaturesBar && StyleEditorShowShadesButton != 0;
  const bool showNeighborsBtn =
      showColorFeaturesBar && StyleEditorShowNeighborsButton != 0;
  const bool showBlendBtn =
      showColorFeaturesBar && StyleEditorShowBlendButton != 0;
  const bool showMixerBtn =
      showColorFeaturesBar && StyleEditorShowMixerButton != 0;
  auto uncheckFeature = [](QToolButton *btn) {
    if (!btn || !btn->isChecked()) return;
    bool blocked = btn->blockSignals(true);
    btn->setChecked(false);
    btn->blockSignals(blocked);
  };
  if (m_collectorBtn) {
    m_collectorBtn->setVisible(showCollectorBtn);
    if (!showCollectorBtn) uncheckFeature(m_collectorBtn);
  }
  if (m_historyBtn) {
    m_historyBtn->setVisible(showHistoryBtn);
    if (!showHistoryBtn) uncheckFeature(m_historyBtn);
  }
  if (m_harmonyBtn) {
    m_harmonyBtn->setVisible(showHarmonyBtn);
    if (!showHarmonyBtn) uncheckFeature(m_harmonyBtn);
  }
  if (m_shadesBtn) {
    m_shadesBtn->setVisible(showShadesBtn);
    if (!showShadesBtn) uncheckFeature(m_shadesBtn);
  }
  if (m_neighborsBtn) {
    m_neighborsBtn->setVisible(showNeighborsBtn);
    if (!showNeighborsBtn) uncheckFeature(m_neighborsBtn);
  }
  if (m_blendBtn) {
    m_blendBtn->setVisible(showBlendBtn);
    if (!showBlendBtn) uncheckFeature(m_blendBtn);
  }
  if (m_mixerBtn) {
    m_mixerBtn->setVisible(showMixerBtn);
    if (!showMixerBtn) uncheckFeature(m_mixerBtn);
  }
  if (!showColorFeaturesBar) {
    uncheckFeature(m_collectorBtn);
    uncheckFeature(m_historyBtn);
    uncheckFeature(m_harmonyBtn);
    uncheckFeature(m_shadesBtn);
    uncheckFeature(m_neighborsBtn);
    uncheckFeature(m_blendBtn);
    uncheckFeature(m_mixerBtn);
  }
  if (m_colorFeaturesBar) m_colorFeaturesBar->setVisible(showColorFeaturesBar);
  syncFeaturePage();
  if (QLayout *lay = m_pickerChrome->layout()) lay->activate();
  m_pickerChrome->updateGeometry();
  m_sectionBar->updateGeometry();
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
  SectionToggleBar *bar = static_cast<SectionToggleBar *>(m_sectionBar);
  if (!bar) return;

  auto addBtn = [&](QAction *action, const QString &label,
                    const QString &tip) -> QToolButton * {
    QToolButton *btn = new QToolButton(bar);
    btn->setCheckable(true);
    btn->setAutoRaise(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 0px; margin: 0px; "
        "min-width: 0px; min-height: 0px; }"
        "QToolButton:hover, QToolButton:checked, QToolButton:checked:hover { "
        "padding: 0px; margin: 0px; }"));
    btn->setText(label);
    btn->setToolTip(tip);
    btn->setChecked(action->isChecked());
    btn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    btn->setMinimumSize(0, 0);
    connect(btn, SIGNAL(toggled(bool)), action, SLOT(setChecked(bool)));
    connect(action, SIGNAL(toggled(bool)), btn, SLOT(setChecked(bool)));
    bar->addButton(btn);
    return btn;
  };

  addBtn(picker, tr("CP"), tr("Color picker"));
  addBtn(alpha, tr("A"), tr("Alpha slider"));
  addBtn(hsv, tr("HSV"), tr("HSV sliders"));
  addBtn(rgb, tr("RGB"), tr("RGB sliders"));
  addBtn(hex, tr("HEX"), tr("Hex"));
  m_varBtn = addBtn(swatch, tr("VAR"), tr("Color variations"));
  bar->addButton(m_wheelKindBtn);
  bar->addButton(m_rectKindBtn);
  bar->addButton(m_advancedModeBtn);
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
  if (m_historyGrid)
    static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
}

//-----------------------------------------------------------------------------

void PlainColorPage::onControlChanged(const ColorModel &color,
                                      bool isDragging) {
  if (!(m_color == color)) {
    m_color = color;
    updateControls();
  }

  if (m_signalEnabled) emit colorChanged(m_color, isDragging);
  if (!isDragging && m_historyGrid)
    static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
}

//-----------------------------------------------------------------------------

void PlainColorPage::onWheelChanged(const ColorModel &color, bool isDragging) {
  if (!(m_color == color)) {
    m_color = color;
    updateControls();
  }
  if (m_signalEnabled) emit colorChanged(m_color, isDragging);
  if (!isDragging && m_historyGrid)
    static_cast<ColorHistoryGrid *>(m_historyGrid)->push(m_color);
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
  QMenu *colorFeaturesMenu = menu->addMenu(tr("Color Features Bar"));
  QAction *showColorFeaturesBar =
      colorFeaturesMenu->addAction(tr("Show Color Features Bar"));
  showColorFeaturesBar->setCheckable(true);
  showColorFeaturesBar->setData(QStringLiteral("chrome:colorFeatures"));
  showColorFeaturesBar->setChecked(StyleEditorShowColorFeaturesBar != 0);
  QAction *showCollectorBtn =
      colorFeaturesMenu->addAction(tr("Show Color Collector"));
  showCollectorBtn->setCheckable(true);
  showCollectorBtn->setData(QStringLiteral("chrome:collector"));
  showCollectorBtn->setChecked(StyleEditorShowCollectorButton != 0);
  QAction *showHistoryBtn = colorFeaturesMenu->addAction(tr("Show Color History"));
  showHistoryBtn->setCheckable(true);
  showHistoryBtn->setData(QStringLiteral("chrome:history"));
  showHistoryBtn->setChecked(StyleEditorShowHistoryButton != 0);
  QAction *showHarmonyBtn = colorFeaturesMenu->addAction(tr("Show Color Harmonies"));
  showHarmonyBtn->setCheckable(true);
  showHarmonyBtn->setData(QStringLiteral("chrome:harmony"));
  showHarmonyBtn->setChecked(StyleEditorShowHarmonyButton != 0);
  QAction *showShadesBtn = colorFeaturesMenu->addAction(tr("Show Color Shades"));
  showShadesBtn->setCheckable(true);
  showShadesBtn->setData(QStringLiteral("chrome:shades"));
  showShadesBtn->setChecked(StyleEditorShowShadesButton != 0);
  QAction *showNeighborsBtn =
      colorFeaturesMenu->addAction(tr("Show Neighboring Colors"));
  showNeighborsBtn->setCheckable(true);
  showNeighborsBtn->setData(QStringLiteral("chrome:neighbors"));
  showNeighborsBtn->setChecked(StyleEditorShowNeighborsButton != 0);
  QAction *showBlendBtn =
      colorFeaturesMenu->addAction(tr("Show Intermediate Colors"));
  showBlendBtn->setCheckable(true);
  showBlendBtn->setData(QStringLiteral("chrome:blend"));
  showBlendBtn->setChecked(StyleEditorShowBlendButton != 0);
  QAction *showMixerBtn = colorFeaturesMenu->addAction(tr("Show Color Mixer"));
  showMixerBtn->setCheckable(true);
  showMixerBtn->setData(QStringLiteral("chrome:mixer"));
  showMixerBtn->setChecked(StyleEditorShowMixerButton != 0);
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
  } else if (key == QStringLiteral("chrome:colorFeatures")) {
    StyleEditorShowColorFeaturesBar = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:collector")) {
    StyleEditorShowCollectorButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:history")) {
    StyleEditorShowHistoryButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:harmony")) {
    StyleEditorShowHarmonyButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:shades")) {
    StyleEditorShowShadesButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:neighbors")) {
    StyleEditorShowNeighborsButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:blend")) {
    StyleEditorShowBlendButton = chosen->isChecked() ? 1 : 0;
    m_plainColorPage->updatePickerChrome();
  } else if (key == QStringLiteral("chrome:mixer")) {
    StyleEditorShowMixerButton = chosen->isChecked() ? 1 : 0;
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
