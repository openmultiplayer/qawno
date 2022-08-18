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

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QRegExp>
#include <QSettings>
#include <QUndoStack>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>

#include "AboutDialog.h"
#include "Compiler.h"
#include "CompilerSettingsDialog.h"
#include "ServerSettingsDialog.h"
#include "EditorWidget.h"
#include "FindDialog.h"
#include "GoToDialog.h"
#include "MainWindow.h"
#include "OutputWidget.h"
#include "ReplaceDialog.h"
#include "StatusBar.h"

#include "ui_MainWindow.h"
#include <Windows.h>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent),
    ui_(new Ui::MainWindow),
    editors_(),
    server_(),
    fileNames_(),
    mru_()
{
  ui_->setupUi(this);
  ui_->tabWidget->setTabsClosable(true);

  setStatusBar(new StatusBar(this));

  QSettings settings;

  defaultPalette = ui_->splitter->palette();

  QPalette darkPalette;
  darkPalette.setColor(QPalette::Window, QColor(0x282C34));
  darkPalette.setColor(QPalette::WindowText, Qt::white);
  darkPalette.setColor(QPalette::Base, QColor(0x282C34));
  darkPalette.setColor(QPalette::AlternateBase, QColor(0x282C34));
  darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
  darkPalette.setColor(QPalette::ToolTipText, Qt::white);
  darkPalette.setColor(QPalette::Text, Qt::white);
  darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::ButtonText, Qt::white);
  darkPalette.setColor(QPalette::BrightText, Qt::red);
  darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::Highlight, QColor(0x324054));
  darkPalette.setBrush(QPalette::HighlightedText, QBrush(Qt::NoBrush));

  darkModePalette = darkPalette;

  bool useDarkMode = settings.value("DarkMode", false).toBool();
  ui_->actionDarkMode->setChecked(useDarkMode);
  
  bool useMRU = settings.value("MRU", false).toBool();
  ui_->actionMRU->setChecked(useMRU);

  if (useDarkMode) {
    ui_->splitter->setPalette(darkModePalette);
    ui_->tabWidget->setPalette(darkModePalette);
  } else {
    ui_->splitter->setPalette(defaultPalette);
    ui_->tabWidget->setPalette(defaultPalette);
  }

  resize(settings.value("WindowSize", QSize(800, 600)).toSize());
  if (settings.value("Maximized", false).toBool()) {
    setWindowState(Qt::WindowMaximized);
  }

  if (QApplication::instance()->arguments().size() > 1) {
    loadFile(QApplication::instance()->arguments()[1]);
  } else {
    QStringList lastOpenedFileNames = settings.value("LastFiles").toStringList();
    if (lastOpenedFileNames.count() == 0) {
      createTab("");
    } else for (auto const & i : lastOpenedFileNames) {
      if (!i.isEmpty()) {
        loadFile(i);
      }
    }
  }
  connect(ui_->tabWidget, SIGNAL(currentChanged(int)), SLOT(currentChanged(int)));
  connect(ui_->tabWidget, SIGNAL(tabCloseRequested(int)), SLOT(tabCloseRequested(int)));
  connect(ui_->functions, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));
  QApplication::instance()->installEventFilter(this);

  loadNativeList();

  updateTitle();
}

MainWindow::~MainWindow() {
  QSettings settings;

  settings.setValue("Maximized", isMaximized());
  if (!isMaximized()) {
    settings.setValue("WindowSize", size());
  }

  delete ui_;
}

void MainWindow::loadNativeList() {
  // Declare an invalid symbol for list items that aren't real symbols.
  static QString unused("=");
  QListWidgetItem* child;
  QFont* fileFont = new QFont("Sans Serif", 20, 2);
  QFont* funcFont = new QFont("Sans Serif", 12, 2);
  QFont* headFont = new QFont("Sans Serif", 12, 2);
  // Loop through all `includes/*.inc` files (ensure they aren't directories).
  QDir includes("./include", "*.inc", QDir::IgnoreCase, QDir::Files | QDir::Readable);
  for (auto const & fileName : includes.entryInfoList()) {
    QFile f{fileName.absoluteFilePath()};
    if (f.open(QFile::ReadOnly | QFile::Text)) {
      child = new QListWidgetItem("\n" + fileName.fileName() + "\n", ui_->functions);
      child->setFont(*fileFont);
      child->setTextAlignment(4);
      child->setFlags(child->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
      // Pad the natives list so indexing works, though we never see this in the status bar.
      natives_.push_back(unused);
      // Find every line that starts with `native`.
      while (!f.atEnd()) {
        QByteArray line = f.readLine();
        // Skip leading whitespace.
        int idx = 0, len = line.size();
        char const* data = line.constData();
        while (idx < len) {
          if (data[idx] > ' ') {
            break;
          }
          ++idx;
        }
        if (idx < len) {
          if (strncmp(data + idx, "native ", 7) == 0) {
            idx += 7;

            // Work forwards to the start of text.
            while (data[idx] <= ' ') {
              if (data[idx] == '\n' || data[idx] == '\r') {
                goto not_a_native;
              }
              ++idx;
            }

            // Work back to the end of the parameters (skips `= other;` too).
            do {
              --len;
              if (len == idx) {
                goto not_a_native;
              }
            } while (data[len] != ')');

            // Extract the full name, return, and parameters.
            natives_.push_back(QString(std::string(data + idx, (size_t)len - idx + 1).c_str()));

            // Extract just the name.
            len = idx;
            for ( ; ; ) {
              if (data[len] == '\n' || data[len] == '\r') {
                // Remove this from the list again.  We can't add the name after here because we've
                // clobbered the indexes.
                natives_.pop_back();
                goto not_a_native;
              }
              if (data[len] == ':') {
                // Skip the return tag.
                idx = len + 1;
              }
              if (data[len] == '(') {
                // Found the parameters start.
                break;
              }
              ++len;
            }
            // Found some function name.
            while (data[idx] <= ' ') {
              ++idx;
            }
            do {
              --len;
            }
            while (data[len] <= ' ');
            ++len;
            if (idx < len) {
              if (data[idx] == '#') {
                // Special syntax:
                //
                //   native #Heading();
                //
                // Purely for Qawno titles.
                child = new QListWidgetItem("\n" + QString(std::string(data + idx + 1, (size_t)len - idx - 1).c_str()) + "\n", ui_->functions);
                child->setFont(*headFont);
                child->setTextAlignment(4);
                child->setFlags(child->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
                natives_.pop_back();
                natives_.push_back(unused);
              } else {
                QString name(std::string(data + idx, (size_t)len - idx).c_str());
                child = new QListWidgetItem(name, ui_->functions);
                child->setFont(*funcFont);
                // Add the native to the list of auto-complete predictions with default likelihood.
                predictions_.insert(name, { 1, 1 });
              }
            } else {
              natives_.pop_back();
            }
          }
        }
not_a_native:
        (void)0;
      }
    }
  }
}

void MainWindow::currentChanged(int index) {
  hidePopup();
  startWord();
  if (index != -1) {
    // Remove this index from the MRU list.
    mru_.removeAll(index);
    // And push it to the top.
    mru_.push(index);
  }
  updateTitle();
}

void MainWindow::tabCloseRequested(int index) {
  ui_->tabWidget->setCurrentIndex(index);
  on_actionClose_triggered();
}

void MainWindow::currentRowChanged(int index) {
  if (index == -1) {
    if (!getCurrentEditor()) {
      return;
    }
    QTextCursor cursor = getCurrentEditor()->textCursor();
    int line = cursor.blockNumber() + 1;
    int column = cursor.columnNumber() + 1;
    int selected = cursor.selectionEnd() - cursor.selectionStart();
    dynamic_cast<StatusBar*>(statusBar())->setCursorPosition(line, column, selected);
    statusBar()->showMessage("");
  } else {
    statusBar()->showMessage(natives_[index]);
  }
}

void MainWindow::hidePopup() {
  if (popup_) {
    popup_->hide();
    delete popup_;
    popup_ = nullptr;
  }
}

void MainWindow::startWord() {
  // Initialise.
  wordStart_ = -1;
  wordEnd_ = -1;
  // We are typing or clicked somewhere new.  Save the extents of the current symbol.
  EditorWidget* editor = getCurrentEditor();
  if (!editor) {
    return;
  }
  QTextCursor cursor = editor->textCursor();
  if (cursor.hasSelection()) {
    return;
  }
  int start = cursor.selectionStart();
  int end = start;
  QString text = editor->toPlainText();
  QChar const* data = text.constData();
  int len = 0;
  while (start) {
    QChar ch = data[--start];
    // Test if the current character is a valid symbol character.
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '@') {
      ++len;
    } else {
      ++start;
      break;
    }
  }
  while (end < text.length()) {
    QChar ch = data[end++];
    // Test if the current character is a valid symbol character.
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '@') {
      ++len;
    } else {
      --end;
      break;
    }
  }
  QChar ch = data[start];
  // Test if the first character is a valid initial symbol character (i.e. not a number).
  if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '@') {
    // Yes.
    wordStart_ = start;
    wordEnd_ = end;
    // Save the current word at this position so that when we edit it we can adjust the predictions list.
    initialWord_ = QString(data + start, end - start);
  } else if ((ch >= '0' && ch <= '9')) {
    // It is a number, which is like a symbol in many ways, but without auto-predict.
    wordStart_ = start;
  }
}

void MainWindow::on_editor_textChanged() {
  updateTitle();

  // Called when the current text changes, every time.  We may need to debounce this a little bit
  // because we are going to be scanning through a long list of strings every keypress otherwise.
  hidePopup();
  // Remove the current word from the predictions list, since we're changing it.
  if (wordEnd_ != -1 && prevEnd_ != -1) {
    if (predictions_.contains(prevWord_)) {
      predictions_s& prediction = predictions_[prevWord_];
      if (prediction.Count < 2) {
        predictions_.remove(prevWord_);
      } else {
        --prediction.Count;
      }
    }
  }
  if (wordEnd_ == -1) {
    // Not in a symbol, don't care.
  }
  // Get the current editor.
  EditorWidget* editor = getCurrentEditor();
  // Get the current cursor position and the current text.
  int pos = editor->textCursor().selectionStart();
  QString text = editor->toPlainText();
  QChar const* data = text.constData();
  // Check if the character typed starts or continues a new word.
  // We now have the extent of the current symbol.  If it is more than three characters and starts
  // with a non-number, search for auto-complete matches.
  int searchLen = pos - wordStart_;
  if (searchLen >= 3) {
    // Loop through all the known symbols.
    suggestions_.clear();
    for (auto it = predictions_.constBegin(), end = predictions_.constEnd(); it != end; ++it) {
      auto const& name = it.key();
      int matchLen = name.length();
      if (matchLen >= searchLen) {
        for (int i = wordStart_, j = 0; j != matchLen; ++j) {
          // Case-insensitive comparison.
          if (name[j].toUpper() == data[i].toUpper()) {
            ++i;
            if (i == pos) {
              // We've found a candidate, all the characters from `data` (the word currently
              // being typed).  We sort the matches by `j`, so that the ones that take the fewest
              // characters to match come first (so `Get` first lists the actual `Get` functions,
              // before things like `TogglePlayerScoresPingsUpdate` which just happen to have `g`,
              // `e`, and `t` somewhere in that order.  We also store "likelihood" metrics with
              // the names, so that those symbols that are used more move up the list quickly.
              // Probably double the likelihood every time a symbol is selected and subtract this
              // value from the length.
              // Get the final sort position.
              suggestions_.push_back({ &name, j - it.value().Rank });
              break;
            }
          }
        }
      }
    }
    if (suggestions_.size()) {
      // There are some suggestions.  Sort them.
      std::sort(suggestions_.begin(), suggestions_.end());
      // Determine where to draw the suggestions box.
      QRect rect = editor->cursorRect();
      popup_ = new QListWidget(editor);
      popup_->setParent(editor);
      popup_->setGeometry(rect.right() + 60, rect.bottom(), 170, 200);
      for (auto const& it : suggestions_) {
        QListWidgetItem* cur = new QListWidgetItem(*it.Name, popup_);
      }
      popup_->show();
    }
  }
  // Add this word to the predictions list.  AFTER showing suggestions so the thing we just typed
  // doesn't affect the results.  This gets called randomly when files load and the cursors are
  // initialised - this is a bug I'm not going to fix, it just means a few random words will be
  // predicted even if you delete them all.  Actually, I can fix this by setting the initial state
  // of the file parser to `NUMBER` to ignore those initial words since they're already added once.
  if (initialWord_.length() < 3) {
    (void)0;
  } else if (predictions_.contains(initialWord_)) {
    ++predictions_[initialWord_].Count;
  } else {
    predictions_.insert(initialWord_, { 1, 1 });
  }
}

void MainWindow::finishSymbol(QString const& symbol, bool add) {
  // Add or remove the symbol to the predictions list.
  if (symbol.length() < 3) {
    (void)0;
  } else if (add) {
    if (predictions_.contains(symbol)) {
      ++predictions_[symbol].Count;
    } else {
      predictions_.insert(symbol, { 1, 1 });
    }
  } else {
    if (predictions_.contains(symbol)) {
      predictions_s& prediction = predictions_[symbol];
      if (prediction.Count < 2) {
        predictions_.remove(symbol);
      } else {
        --prediction.Count;
      }
    }
  }
}

enum file_parse_state_e {
  file_parse_state_number,
  file_parse_state_symbol,
  file_parse_state_unknown,
  file_parse_state_line_comment,
  file_parse_state_block_comment,
  file_parse_state_character,
  file_parse_state_string,
  file_parse_state_preproc,
};

void MainWindow::parseFile(QString const text, bool add) {
  // When adding we start in parse state `number` because any symbols at position 0 are already
  // added to the predictions list by the text edit callback.
  file_parse_state_e state = add ? file_parse_state_number : file_parse_state_unknown;
  QChar const* data = text.data();
  int len = text.length();
  QString symbol = "";
  bool escape = false;
  while (len--) {
    QChar ch = *data++;
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '@') {
      switch (state) {
      case file_parse_state_symbol:
        // Add this character to the current symbol.
        symbol += ch;
        break;
      case file_parse_state_unknown:
        // Start a new symbol.
        symbol = ch;
        state = file_parse_state_symbol;
        break;
      }
    } else if ((ch >= '0' && ch <= '9')) {
      switch (state) {
      case file_parse_state_symbol:
        // Add this character to the current symbol.
        symbol += ch;
        break;
      case file_parse_state_unknown:
        // Start a new number.
        state = file_parse_state_number;
        break;
      }
    } else if (ch == '/') {
      if (len && *data == '/') {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
        case file_parse_state_unknown:
          state = file_parse_state_line_comment;
          ++data;
          --len;
          break;
        }
      } else if (len && *data == '*') {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
        case file_parse_state_unknown:
          state = file_parse_state_block_comment;
          ++data;
          --len;
          break;
        }
      } else {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
          state = file_parse_state_unknown;
          break;
        }
      }
    } else if (ch == '*') {
      if (len && *data == '/') {
        switch (state) {
        case file_parse_state_block_comment:
          state = file_parse_state_unknown;
          ++data;
          --len;
          break;
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
          state = file_parse_state_unknown;
          break;
        }
      } else {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
          state = file_parse_state_unknown;
          break;
        }
      }
    } else if (ch == '#') {
      switch (state) {
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
        state = file_parse_state_preproc;
        break;
      }
    } else if (ch == '"') {
      switch (state) {
      case file_parse_state_string:
        if (!escape) {
          state = file_parse_state_unknown;
        }
        break;
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_unknown:
        state = file_parse_state_string;
        break;
      }
    } else if (ch == '\'') {
      switch (state) {
      case file_parse_state_character:
        if (!escape) {
          state = file_parse_state_unknown;
        }
        break;
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_unknown:
        state = file_parse_state_character;
        break;
      }
    } else if (ch == '\\') {
      switch (state) {
      case file_parse_state_string:
      case file_parse_state_character:
        escape = !escape;
        // Don't reset `escape` at the end of the loop.
        continue;
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_unknown:
        state = file_parse_state_character;
        break;
      }
    } else if (ch == '\r' || ch == '\n') {
      switch (state) {
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_line_comment:
      case file_parse_state_character:
      case file_parse_state_preproc:
        state = file_parse_state_unknown;
        break;
      }
    } else {
      switch (state) {
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
        state = file_parse_state_unknown;
        break;
      }
    }
    escape = false;
  }
}

void MainWindow::replaceSuggestion() {
  QString const& replacement = popup_->currentRow() == -1 ? popup_->item(0)->text() : popup_->currentItem()->text();
  EditorWidget* editor = getCurrentEditor();
  if (!editor) {
    return;
  }
  QTextCursor cursor = editor->textCursor();
  if (cursor.hasSelection()) {
    return;
  }
  // Get the current cursor position and the current text.
  int start = cursor.selectionStart();
  QString text = editor->toPlainText();
  QChar const* data = text.constData();
  int len = 0;
  while (start--) {
    QChar ch = data[start];
    // Test if the current character is a valid symbol character.
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '@') {
      ++len;
    } else {
      break;
    }
  }
  ++start;
  // We now have the extent of the current symbol.  If it is more than three characters and starts
  // with a non-number, search for auto-complete matches.
  cursor.setPosition(start, QTextCursor::MoveAnchor);
  cursor.setPosition(start + len , QTextCursor::KeepAnchor);
  cursor.insertText(replacement);
  // Increase how popular this replacement is.
  predictions_[replacement] = { predictions_[replacement].Rank * 2, predictions_[replacement].Count };
  hidePopup();
  startWord();
}

void MainWindow::on_actionNew_triggered() {
  createTab("");
}

void MainWindow::on_actionOpen_triggered() {
  QSettings settings;
  QString dir = settings.value("LastOpenDir").toString();

  QString caption = tr("Open File");
  QString filter = tr("Pawn scripts (*.pwn *.inc)");
  QString fileName = QFileDialog::getOpenFileName(this, caption, dir, filter);

  bool close = fileNames_.count() == 1 && isNewFile() && !isFileModified();
  // Is the file already open?  If so just switch to the tab.
  for (int i = fileNames_.count(); i--; ) {
    if (fileNames_[i] == fileName) {
      ui_->tabWidget->setCurrentIndex(i);
      return;
    }
  }

  if (!loadFile(fileName)) {
    return;
  }

  if (close) {
    // Opening a first file replaces the initial new file.
    editors_.remove(0);
    fileNames_.removeAt(0);
    ui_->tabWidget->removeTab(0);
  }

  dir = QFileInfo(fileName).dir().path();
  settings.setValue("LastOpenDir", dir);
  QStringList files {};
  for (auto const & i : fileNames_) {
    files.push_back(i);
  }
  settings.setValue("LastFiles", files);
}

void MainWindow::on_actionPaste_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->paste();
  }
}

void MainWindow::on_actionCopy_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->copy();
  }
}

void MainWindow::on_actionCut_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->cut();
  }
}

void MainWindow::on_actionRedo_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->redo();
  }
}

void MainWindow::on_actionUndo_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->undo();
  }
}

void MainWindow::scrollByLines(int n) {
  if (auto editor = getCurrentEditor()) {
    editor->verticalScrollBar()->setValue(editor->verticalScrollBar()->value() + n);
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  int count = ui_->tabWidget->count();
  switch (event->type()) {
  case QKeyEvent::KeyPress:
    switch (static_cast<QKeyEvent*>(event)->key()) {
    case Qt::Key_Down:
      if (popup_) {
        popup_->setCurrentRow(popup_->currentRow() + 1);
        return true;
      } else if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Scroll up.
        scrollByLines(1);
        return true;
      }
      break;
    case Qt::Key_Up:
      if (popup_) {
        popup_->setCurrentRow(popup_->currentRow() - 1);
        return true;
      } else if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Scroll down.
        scrollByLines(-1);
        return true;
      }
      break;
    case Qt::Key_D:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        if (auto editor = getCurrentEditor()) {
          // Duplicate the line.
          QTextCursor cursor = editor->textCursor();
          int position = cursor.position();
          int midpoint = cursor.positionInBlock();
          QString text = cursor.block().text();
          cursor.insertText(text.right(text.length() - midpoint));
          cursor.insertText("\n");
          cursor.insertText(text.left(midpoint));
          cursor.setPosition(position);
          editor->setTextCursor(cursor);
        }
        return true;
      }
      break;
    case Qt::Key_L:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        if (auto editor = getCurrentEditor()) {
          // Delete the line.
          QTextCursor cursor = editor->textCursor();
          int position = cursor.position();
          int midpoint = cursor.positionInBlock();
          cursor.setPosition(position - midpoint, QTextCursor::MoveAnchor);
          cursor.setPosition(position + cursor.block().length() - midpoint, QTextCursor::KeepAnchor);
          cursor.insertText("");
        }
        return true;
      }
      break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
      if (popup_) {
        replaceSuggestion();
        return true;
      }
      break;
    case Qt::Key_Escape:
      if (popup_) {
        hidePopup();
        return true;
      }
      break;
    case Qt::Key_Tab:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Tab switcher forwards.
        if (ui_->actionMRU->isChecked()) {
          ++mruIndex_;
          if (mruIndex_ == count) {
            mruIndex_ = 0;
          }
          ui_->tabWidget->setCurrentIndex(mru_[count - 1 - mruIndex_]);
        } else {
          ui_->tabWidget->setCurrentIndex((getCurrentIndex() + 1) % count);
        }
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_Backtab:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Tab switcher backwards.
        if (ui_->actionMRU->isChecked()) {
          --mruIndex_;
          if (mruIndex_ == -1) {
            mruIndex_ = count - 1;
          }
          ui_->tabWidget->setCurrentIndex(mru_[count - 1 - mruIndex_]);
        } else {
          ui_->tabWidget->setCurrentIndex((getCurrentIndex() - 1 + count) % count);
        }
        // Stop propagation.
        return true;
      }
      break;
    }
    break;
  case QKeyEvent::KeyRelease:
    switch (static_cast<QKeyEvent*>(event)->key()) {
    case Qt::Key_Control:
      // If they release ctrl, reset the MRU switcher, regardless of what else is happening.
      mruIndex_ = 0;
      break;
    }
    break;
  }
  // Pass it on.
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::on_actionClose_triggered() {
  hidePopup();

  bool canClose = true;
  int cur = getCurrentIndex();
  if (cur < 0) {
    return;
  }

  if (isFileModified() && !isFileEmpty()) {
    QString message = (!isNewFile())
      ? tr("Save changes to %1?").arg(fileNames_.at(cur))
      : tr("Save changes to a new file?");
    int result = QMessageBox::question(this,
                                       QCoreApplication::applicationName(),
                                       message,
                                       QMessageBox::Yes | QMessageBox::No
                                                        | QMessageBox::Cancel);
    switch (result) {
      case QMessageBox::Yes:
        on_actionSave_triggered();
        canClose = !isFileModified();
        break;
      case QMessageBox::No:
        canClose = true;
        break;
      case QMessageBox::Cancel:
        canClose = false;
        break;
    }
  }

  if (canClose) {
    parseFile(getCurrentEditor()->toPlainText(), false);
    editors_.remove(cur);
    fileNames_.removeAt(cur);
    QStringList files {};
    for (auto const & i : fileNames_) {
      files.push_back(i);
    }
    QSettings settings;
    settings.setValue("LastFiles", files);
    ui_->tabWidget->removeTab(cur);

    if (fileNames_.count() == 0) {
      createTab("");
    }
  }

  updateTitle();
  startWord();
}

void MainWindow::on_actionQuit_triggered() {
  bool canClose = true;
  int cur = getCurrentIndex();

  for (int i = 0, j = ui_->tabWidget->count(); i != j; ++i) {
    ui_->tabWidget->setCurrentIndex(i);
    if (isFileModified() && !isFileEmpty()) {
      QString message = (!isNewFile())
        ? tr("Save changes to %1?").arg(fileNames_.at(i))
        : tr("Save changes to a new file?");
      int result = QMessageBox::question(this,
                                         QCoreApplication::applicationName(),
                                         message,
                                         QMessageBox::Yes | QMessageBox::No
                                                          | QMessageBox::Cancel);
      switch (result) {
        case QMessageBox::Yes:
          on_actionSave_triggered();
          canClose = canClose && !isFileModified();
          break;
        case QMessageBox::No:
          break;
        case QMessageBox::Cancel:
          canClose = false;
          break;
      }
    }
  }

  if (canClose) {
    QApplication::instance()->quit();
  } else if (cur != -1) {
    ui_->tabWidget->setCurrentIndex(cur);
    updateTitle();
  }
}

void MainWindow::on_actionSave_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  if (isNewFile()) {
    on_actionSaveAs_triggered();
    return;
  }

  QFile file(fileNames_[getCurrentIndex()]);
  if (!file.open(QIODevice::WriteOnly)) {
    QString message =
      tr("Could not save to %1: %2.").arg(fileNames_[getCurrentIndex()], file.errorString());
    QMessageBox::critical(this,
                          QCoreApplication::applicationName(),
                          message,
                          QMessageBox::Ok);
    return;
  }

  file.write(getCurrentEditor()->toPlainText().toLatin1());
  getCurrentEditor()->textChanged();
  setFileModified(false);
}

void MainWindow::on_actionSaveAs_triggered() {
  QSettings settings;
  QString dir = settings.value("LastSaveDir").toString();

  QString caption = tr("Save File As");
  QString filter = tr("Pawn scripts (*.pwn *.inc)");
  QString fileName = QFileDialog::getSaveFileName(this, caption, dir, filter);

  if (fileName.isEmpty()) {
    return;
  }

  dir = QFileInfo(fileName).dir().path();
  settings.setValue("LastSaveDir", dir);
  QStringList files {};
  fileNames_[getCurrentIndex()] = fileName;
  for (auto const & i : fileNames_) {
    files.push_back(i);
  }
  settings.setValue("LastFiles", files);

  ui_->tabWidget->setTabText(getCurrentIndex(), fileName);
  return on_actionSave_triggered();
}

void MainWindow::on_actionFind_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  int found = 0;
  {
    FindDialog dialog;
    dialog.exec();
    found = dialog.result();
  }
  switch (found) {
  case 1:
    // "Find".
    findStart_ = getCurrentEditor()->textCursor().position();
    findRound_ = 0;
    on_actionFindNext_triggered();
    break;
  case 2:
    // "Replace".
    findStart_ = getCurrentEditor()->textCursor().position();
    findRound_ = 0;
    on_actionReplaceNext_triggered();
    break;
  case 3:
    // "All".
    findStart_ = getCurrentEditor()->textCursor().position();
    findRound_ = 0;
    on_actionReplaceAll_triggered();
    break;
  }
}

void MainWindow::on_actionFindNext_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  FindDialog dialog;
  QTextDocument::FindFlags flags;

  if (dialog.matchCase()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  if (dialog.matchWholeWords()) {
    flags |= QTextDocument::FindWholeWords;
  }
  if (dialog.searchBackwards()) {
    flags |= QTextDocument::FindBackward;
  }

  QTextCursor current = getCurrentEditor()->textCursor();
  QTextCursor next;

  if (dialog.useRegExp()) {
    Qt::CaseSensitivity sens = dialog.matchCase()? Qt::CaseSensitive:
                                                   Qt::CaseInsensitive;
    QRegExp regexp(dialog.findWhatText(), sens);
    next = getCurrentEditor()->document()->find(regexp, current, flags);
  } else {
    next = getCurrentEditor()->document()->find(dialog.findWhatText(), current, flags);
  }

  bool found = !next.isNull() &&
    (findRound_ == 0 || next.position() < findStart_);

  if (!found && findRound_ > 0) {
    QString string = dialog.findWhatText();
    QString message = tr("No matching text found for \"%1\".").arg(string);
    QMessageBox::information(this,
                              QCoreApplication::applicationName(),
                              message,
                              QMessageBox::Ok);
    findStart_ = next.position();
    findRound_ = 0;
    return;
  }

  if (!found && findRound_ == 0) {
    next = current;
    if (dialog.searchBackwards()) {
      next.movePosition(QTextCursor::End);
    } else {
      next.movePosition(QTextCursor::Start);
    }
  }

  getCurrentEditor()->setTextCursor(next);

  if (!found && findRound_ == 0) {
    findRound_++;
    on_actionFindNext_triggered();
  }
}

void MainWindow::on_actionReplaceNext_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  FindDialog dialog;
  QTextDocument::FindFlags flags;

  if (dialog.matchCase()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  if (dialog.matchWholeWords()) {
    flags |= QTextDocument::FindWholeWords;
  }
  if (dialog.searchBackwards()) {
    flags |= QTextDocument::FindBackward;
  }

  QTextCursor current = getCurrentEditor()->textCursor();
  QTextCursor next;

  if (dialog.useRegExp()) {
    Qt::CaseSensitivity sens = dialog.matchCase()? Qt::CaseSensitive:
                                                   Qt::CaseInsensitive;
    QRegExp regexp(dialog.findWhatText(), sens);
    next = getCurrentEditor()->document()->find(regexp, current, flags);
  } else {
    next = getCurrentEditor()->document()->find(dialog.findWhatText(), current, flags);
  }

  bool found = !next.isNull() &&
    (findRound_ == 0 || next.position() < findStart_);

  if (!found && findRound_ > 0) {
    QString string = dialog.findWhatText();
    QString message = tr("No matching text found for \"%1\".").arg(string);
    QMessageBox::information(this,
                              QCoreApplication::applicationName(),
                              message,
                              QMessageBox::Ok);
    findStart_ = next.position();
    findRound_ = 0;
    return;
  }

  if (!found && findRound_ == 0) {
    next = current;
    if (dialog.searchBackwards()) {
      next.movePosition(QTextCursor::End);
    } else {
      next.movePosition(QTextCursor::Start);
    }
  }

  getCurrentEditor()->setTextCursor(next);
  
  if (next.hasSelection()) {
    next.insertText(dialog.replaceText());
  }

  if (!found && findRound_ == 0) {
    findRound_++;
    on_actionReplaceNext_triggered();
  }
}

void MainWindow::on_actionReplaceAll_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  FindDialog dialog;
  QTextDocument::FindFlags flags;
  bool first = true;

  if (dialog.matchCase()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  if (dialog.matchWholeWords()) {
    flags |= QTextDocument::FindWholeWords;
  }
  if (dialog.searchBackwards()) {
    flags |= QTextDocument::FindBackward;
  }

  for ( ; ; ) {
    QTextCursor current = getCurrentEditor()->textCursor();
    QTextCursor next;

    if (dialog.useRegExp()) {
      Qt::CaseSensitivity sens = dialog.matchCase()? Qt::CaseSensitive:
                                                     Qt::CaseInsensitive;
      QRegExp regexp(dialog.findWhatText(), sens);
      next = getCurrentEditor()->document()->find(regexp, current, flags);
    } else {
      next = getCurrentEditor()->document()->find(dialog.findWhatText(), current, flags);
    }

    bool found = !next.isNull() &&
      (findRound_ == 0 || next.position() < findStart_);

    if (!found && findRound_ > 0) {
      QString string = dialog.findWhatText();
      QString message = tr("All instances replaced.");
      QMessageBox::information(this,
                                QCoreApplication::applicationName(),
                                message,
                                QMessageBox::Ok);
      findStart_ = next.position();
      findRound_ = 0;
      return;
    }

    if (!found && findRound_ == 0) {
      next = current;
      if (dialog.searchBackwards()) {
        next.movePosition(QTextCursor::End);
      } else {
        next.movePosition(QTextCursor::Start);
      }
    }

    getCurrentEditor()->setTextCursor(next);
    
    if (next.hasSelection()) {
      if (first) {
        next.beginEditBlock();
        first = false;
      } else {
        next.joinPreviousEditBlock();
      }
      next.insertText(dialog.replaceText());
      next.endEditBlock();
    }

    if (!found && findRound_ == 0) {
      findRound_++;
    }
  }
}

void MainWindow::on_actionGoToLine_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  GoToDialog dialog;
  dialog.exec();
  getCurrentEditor()->jumpToLine(dialog.targetLineNumber());
}

void MainWindow::on_actionEditorFont_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  QFontDialog fontDialog(this);

  bool ok = false;
  QFont newFont = fontDialog.getFont(&ok, getCurrentEditor()->font(), this,
                                     tr("Editor Font"));

  if (ok) {
    getCurrentEditor()->setFont(newFont);
  }
}

void MainWindow::on_actionOutputFont_triggered() {
  QFontDialog fontDialog(this);

  bool ok = false;
  QFont newFont = fontDialog.getFont(&ok, ui_->output->font(), this,
  tr("Output Font"));

  if (ok) {
    ui_->output->setFont(newFont);
  }
}

void MainWindow::on_actionDarkMode_triggered() {
  QSettings settings;
  bool useDarkMode = !settings.value("DarkMode", false).toBool();
  settings.setValue("DarkMode", useDarkMode);
  for (auto i : editors_) {
    i->toggleDarkMode(useDarkMode);
  }
  if (useDarkMode) {
    ui_->splitter->setPalette(darkModePalette);
    ui_->tabWidget->setPalette(darkModePalette);
  } else {
    ui_->splitter->setPalette(defaultPalette);
    ui_->tabWidget->setPalette(defaultPalette);
  }
}

void MainWindow::on_actionMRU_triggered() {
  QSettings settings;
  bool useDarkMode = !settings.value("MRU", false).toBool();
  settings.setValue("MRU", useDarkMode);
}

void MainWindow::on_actionCompiler_triggered() {
  Compiler compiler;
  CompilerSettingsDialog dialog;

  dialog.setCompilerPath(compiler.path());
  dialog.setCompilerOptions(compiler.options().join(" "));

  dialog.exec();

  if (dialog.result() == QDialog::Accepted) {
    compiler.setPath(dialog.compilerPath());
    compiler.setOptions(dialog.compilerOptions());
  }
}

void MainWindow::on_actionServer_triggered() {
  ServerSettingsDialog dialog;

  dialog.setServerPath(server_.path());
  dialog.setServerOptions(server_.options().join(" "));

  dialog.exec();

  if (dialog.result() == QDialog::Accepted) {
    server_.setPath(dialog.serverPath());
    server_.setOptions(dialog.serverOptions());
  }
}

void MainWindow::on_actionSaveAll_triggered() {
  int cur = getCurrentIndex(), count = fileNames_.count();
  if (count == 0) {
    return;
  }
  // Loop over all the tabs and save them all.  There could be include dependencies.
  for (int i = 0; i != count; ++i) {
    ui_->tabWidget->setCurrentIndex(i);
    if (isNewFile()) {
      on_actionSaveAs_triggered();
    } else {
      on_actionSave_triggered();
    }
  }
  // Return to the originally selected tab.
  ui_->tabWidget->setCurrentIndex(cur);
}

void MainWindow::on_actionCompile_triggered() {
  if (fileNames_.isEmpty()) {
    return;
  }
  Compiler compiler;
  const QString& fileName = fileNames_[getCurrentIndex()];
  ui_->output->clear();
  ui_->output->appendPlainText(compiler.commandFor(fileName));
  ui_->output->appendPlainText("\n");
  compiler.run(fileName);
  ui_->output->appendPlainText(compiler.output());
}

void MainWindow::on_actionRun_triggered() {
  if (fileNames_.isEmpty()) {
    return;
  }
  on_actionCompile_triggered();
  server_.run(fileNames_[getCurrentIndex()]);
}

void MainWindow::on_actionAbout_triggered() {
  AboutDialog dialog;
  dialog.exec();
}

void MainWindow::on_actionAboutQt_triggered() {
  QMessageBox::aboutQt(this);
}

void MainWindow::on_editor_cursorPositionChanged() {
  if (!getCurrentEditor()) {
    return;
  }
  hidePopup();
  prevWord_ = initialWord_;
  prevEnd_ = wordEnd_;
  startWord();
  QTextCursor cursor = getCurrentEditor()->textCursor();
  int line = cursor.blockNumber() + 1;
  int column = cursor.columnNumber() + 1;
  int selected = cursor.selectionEnd() - cursor.selectionStart();
  dynamic_cast<StatusBar*>(statusBar())->setCursorPosition(line, column, selected);
}

void MainWindow::closeEvent(QCloseEvent *event) {
  on_actionQuit_triggered();
  if (isFileEmpty()) {
    event->accept();
  } else {
    event->ignore();
  }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent *event) {
  QList<QUrl> urls = event->mimeData()->urls();
  foreach (QUrl url, urls) {
    if (url.isLocalFile()) {
      on_actionClose_triggered();
      if (isFileEmpty()) {
        loadFile(url.toLocalFile());
      }
      break;
    }
  }
}

const QString& MainWindow::getCurrentName() const {
  static QString none = "";
  if (fileNames_.isEmpty()) {
    return none;
  }
  return fileNames_[getCurrentIndex()];
}

EditorWidget* MainWindow::getCurrentEditor() const {
  if (fileNames_.isEmpty()) {
    return 0;
  }
  return editors_[getCurrentIndex()];
}

int MainWindow::getCurrentIndex() const {
  return ui_->tabWidget->currentIndex();
}

void MainWindow::updateTitle() {
  QString title;

  if (fileNames_.isEmpty()) {
    title = "No File";
  } else if (isNewFile()) {
    title = "Untitled File";
  } else {
    title = QFileInfo(fileNames_[getCurrentIndex()]).fileName();
    if (isFileModified()) {
      title.append("*");
    }
  }
  title.append(" - ");
  title.append(QCoreApplication::applicationName());

  setWindowTitle(title);
}

bool MainWindow::loadFile(const QString &fileName) {
  if (fileName.isEmpty()) {
    return false;
  }

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    QString message = tr("Could not open %1: %2.").arg(fileName)
                                                  .arg(file.errorString());
    QMessageBox::critical(this,
                          QCoreApplication::applicationName(),
                          message,
                          QMessageBox::Ok);
    return false;
  }

  createTab(fileName);
  editors_.last()->setPlainText(file.readAll());
  parseFile(editors_.last()->toPlainText(), true);
  setFileModified(false);

  return true;
}

bool MainWindow::isNewFile() const {
  return fileNames_.isEmpty() || fileNames_[getCurrentIndex()].isEmpty();
}

bool MainWindow::isFileModified() const {
  return !editors_.isEmpty() && editors_[getCurrentIndex()]->document()->isModified();
}

void MainWindow::setFileModified(bool isModified) {
  if (!editors_.isEmpty()) {
    editors_[getCurrentIndex()]->document()->setModified(isModified);
    updateTitle();
  }
}

bool MainWindow::isFileEmpty() const {
  if (editors_.isEmpty()) {
    return true;
  }
  QTextDocument *document = editors_[getCurrentIndex()]->document();
  return document->isEmpty()
    || (isNewFile() && !document->toPlainText().contains(QRegExp("\\S")));
}

void MainWindow::createTab(const QString& fileName) {
  mru_.push(ui_->tabWidget->count());
  QWidget* tab = new QWidget();
  tab->setObjectName(fileName);
  QHBoxLayout* horizontalLayout = new QHBoxLayout(tab);
  horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
  horizontalLayout->setMargin(0);
  EditorWidget* editor = new EditorWidget(tab);
  editor->setObjectName(QString::fromUtf8("editor"));
  QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
  sizePolicy2.setHorizontalStretch(0);
  sizePolicy2.setVerticalStretch(0);
  sizePolicy2.setHeightForWidth(editor->sizePolicy().hasHeightForWidth());
  editor->setSizePolicy(sizePolicy2);
  editor->setAcceptDrops(false);
  horizontalLayout->addWidget(editor);
  ui_->tabWidget->addTab(tab, QString());
  ui_->tabWidget->setTabText(ui_->tabWidget->indexOf(tab), fileName);
  bool useDarkMode = ui_->actionDarkMode->isChecked();
  editor->toggleDarkMode(useDarkMode);
  fileNames_.push_back(fileName);
  editors_.push_back(editor);
  editor->focusWidget();
  connect(editor, SIGNAL(textChanged()), SLOT(on_editor_textChanged()));
  connect(editor, SIGNAL(cursorPositionChanged()), SLOT(on_editor_cursorPositionChanged()));
  ui_->tabWidget->setCurrentIndex(ui_->tabWidget->count() - 1);
}

