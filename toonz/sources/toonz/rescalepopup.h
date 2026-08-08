#pragma once

#ifndef RESCALEPOPUP_H
#define RESCALEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include "tgeometry.h"

#include <QThread>
#include <vector>

class QComboBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QGridLayout;

namespace DVGui {
class FileField;
class IntLineEdit;
class LineEdit;
class CheckBox;
class ProgressDialog;
}  // namespace DVGui

namespace ImageUtils {
class FrameTaskNotifier;
}

class RescalePopup final : public DVGui::Dialog {
  Q_OBJECT

public:
  RescalePopup();
  ~RescalePopup();

  void setFiles(const std::vector<TFilePath> &files);
  bool isRunning() const { return m_isRunning; }

public slots:
  void apply();
  void onFinished();
  void onPresetSelected(int index);
  void onWidthEdited();
  void onHeightEdited();
  void onPreserveAspectRatioChanged();
  void onRangeChanged();
  void addPreset();
  void removePreset();

private:
  TDimension targetSize() const;
  TFilePath destinationPath(const TFilePath &source) const;
  void getFrameRange(const TFilePath &source, TFrameId &from,
                     TFrameId &to) const;
  bool checkParameters() const;
  void loadPresetList();
  void savePresetList();
  void updateCurrentSizeDisplay();
  void updatePresetToCustom();
  void syncLinkedSizeFromWidth();
  void syncLinkedSizeFromHeight();
  QString outputBaseName(const TFilePath &source) const;

  class Worker;
  friend class Worker;

  DVGui::IntLineEdit *m_currentWidthFld;
  DVGui::IntLineEdit *m_currentHeightFld;

  DVGui::CheckBox *m_preserveAspectRatio;

  DVGui::FileField *m_saveInFld;
  DVGui::IntLineEdit *m_widthFld;
  DVGui::IntLineEdit *m_heightFld;
  DVGui::LineEdit *m_fileNameFld;
  DVGui::IntLineEdit *m_fromFld;
  DVGui::IntLineEdit *m_toFld;
  DVGui::CheckBox *m_skipExisting;
  QCheckBox *m_removeDotBeforeFrameNumber;
  QComboBox *m_presetCombo;
  QComboBox *m_filterCombo;
  QPushButton *m_addPresetBtn;
  QPushButton *m_removePresetBtn;
  QPushButton *m_okBtn;
  QPushButton *m_cancelBtn;

  DVGui::ProgressDialog *m_progressDialog;
  ImageUtils::FrameTaskNotifier *m_notifier;
  Worker *m_worker;

  QString m_presetListFile;
  TDimension m_currentSize;
  std::vector<TFilePath> m_srcFiles;
  bool m_isRunning;
  bool m_updatingPreset;
  bool m_updatingSize;
};

#endif
