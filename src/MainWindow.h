// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// qawno is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with qawno. If not, see <http://www.gnu.org/licenses/>.

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStack>
#include <QListWidget>
#include "Server.h"
#include "EditorWidget.h"

namespace Ui {
  class MainWindow;
}

class MainWindow: public QMainWindow {
 Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

 private slots:
  void on_actionNew_triggered();
  void on_actionOpen_triggered();
  void on_actionClose_triggered();
  void on_actionQuit_triggered();
  void on_actionSave_triggered();
  void on_actionSaveAs_triggered();
  void on_actionSaveAll_triggered();

  void on_actionPaste_triggered();
  void on_actionCopy_triggered();
  void on_actionCut_triggered();
  void on_actionRedo_triggered();
  void on_actionUndo_triggered();

  void on_actionFind_triggered();
  void on_actionFindNext_triggered();
  void on_actionReplaceNext_triggered();
  void on_actionReplaceAll_triggered();
  void on_actionGoToLine_triggered();

  void on_actionCompile_triggered();
  void on_actionRun_triggered();

  void on_actionEditorFont_triggered();
  void on_actionOutputFont_triggered();

  void on_actionDarkMode_triggered();
  void on_actionMRU_triggered();

  void on_actionCompiler_triggered();
  void on_actionServer_triggered();

  void on_actionAbout_triggered();
  void on_actionAboutQt_triggered();

  void on_editor_textChanged();
  void on_editor_cursorPositionChanged();

  void currentChanged(int index);
  void tabCloseRequested(int index);
  void currentRowChanged(int index);
  void textChanged();
  void hidePopup();
  void finishWord();

 private:
  void updateTitle();
  void replaceSuggestion();
  void loadNativeList();
  bool loadFile(const QString& fileName);
  bool isNewFile() const;
  bool isFileModified() const;
  void setFileModified(bool isModified);
  bool isFileEmpty() const;
  int getCurrentIndex() const;
  const QString& getCurrentName() const;
  EditorWidget* getCurrentEditor() const;
  bool eventFilter(QObject* watched, QEvent* event);

 private:
  Ui::MainWindow *ui_;
  QVector<EditorWidget*> editors_;
  Server server_;

  void createTab(const QString& title);

 private:
  struct suggestions_s {
    QString const* Name;
    int Rank;

    bool operator<(suggestions_s const& right) const {
      if (Rank == right.Rank) {
        // Sort alphabetically.
        return Name->compare(*right.Name) < 0;
      } else {
        // Sort by inverse rank (lowest, potentially negative, first).
        return Rank < right.Rank;
      }
    }
  };

  struct predictions_s {
    int Rank;
    int Count;
  };

  QPalette defaultPalette;
  QPalette darkModePalette;
  QStringList fileNames_;

  // The full name, return, and parameters, of defined natives in the side-panel.  This list exactly
  // matches that list in order, INCLUDING filenames (for simplicity), but they aren't selectable so
  // we can never insert them.
  QStringList natives_;

  // This is shared between all open editors, the neat side-effect being that we can get a cheap and
  // easy way to auto-complete text from custom includes without actually having to parse the
  // transitive includes.  Obviously not all includes, but combined with natives it is a lot.
  QHash<QString, predictions_s> predictions_;
  QVector<suggestions_s> suggestions_;
  QListWidget* popup_ = nullptr;

  // Store the currently edited word for faster lookups.
  int wordStart_ = -1; // The first character of the current symbol, -1 when it isn't a symbol.
  int wordCur_ = -1; // The typing position, matches the cursor always.
  int wordEnd_ = -1; // Same rules as `wordStart_`.
  QString initialWord_; // What the word was before we were editing it.

  // Other data.
  QStack<int> mru_;
  int findStart_ = 0;
  int findRound_ = 0;
  int mruIndex_ = 0;
  QMap<QString, int> words_;
};

#endif // MAINWINDOW_H
