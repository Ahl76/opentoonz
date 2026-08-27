#pragma once

#ifndef FILESELECTION_H
#define FILESELECTION_H

#include "dvitemview.h"
#include "tfilepath.h"

class FileSelection final : public DvItemSelection {
public:
  FileSelection();

  // Disable copy and move operations
  FileSelection(const FileSelection&)            = delete;
  FileSelection& operator=(const FileSelection&) = delete;
  FileSelection(FileSelection&&)                 = delete;
  FileSelection& operator=(FileSelection&&)      = delete;

  // Retrieve currently selected files
  void getSelectedFiles(std::vector<TFilePath>& files);

  // Commands
  void enableCommands() override;

  void duplicateFiles();
  void deleteFiles();
  void copyFiles();
  void pasteFiles();
  void showFolderContents();
  void viewFileInfo();
  void viewFile();
  void convertFiles();
  void rescaleFiles();
  void premultiplyFiles();

  void addToBatchRenderList();
  void addToBatchCleanupList();

  void collectAssets();
  void importScenes();
  void exportScenes();
  void exportScene(TFilePath scenePath);
  void selectAll();
  void separateFilesByColors();
};

#endif  // FILESELECTION_H
