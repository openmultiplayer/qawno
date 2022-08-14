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

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent),
    ui_(new Ui::MainWindow),
    server_(),
    fileNames_()
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

  if (useDarkMode) {
    ui_->splitter->setPalette(darkModePalette);
  } else {
    ui_->splitter->setPalette(defaultPalette);
  }

  resize(settings.value("WindowSize", QSize(800, 600)).toSize());
  if (settings.value("Maximized", false).toBool()) {
    setWindowState(Qt::WindowMaximized);
  }

  if (QApplication::instance()->arguments().size() > 1) {
    loadFile(QApplication::instance()->arguments()[1]);
  } else {
    QString lastOpenedFileName = settings.value("LastFile").toString();
    if (!lastOpenedFileName.isEmpty()) {
      loadFile(lastOpenedFileName);
    }
  }

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

void MainWindow::on_actionNew_triggered() {
  createTab("");
  ui_->tabWidget->setCurrentIndex(editors_.count() - 1);
}

void MainWindow::on_actionOpen_triggered() {
  if (!isFileEmpty()) {
    return;
  }

  QSettings settings;
  QString dir = settings.value("LastOpenDir").toString();

  QString caption = tr("Open File");
  QString filter = tr("Pawn scripts (*.pwn *.inc)");
  QString fileName = QFileDialog::getOpenFileName(this, caption, dir, filter);

  if (!loadFile(fileName)) {
    return;
  }

  dir = QFileInfo(fileName).dir().path();
  settings.setValue("LastOpenDir", dir);
  settings.setValue("LastFile", fileName);
}

void MainWindow::on_actionClose_triggered() {
  bool canClose = true;
  int idx = getCurrentView();
  if (idx < 0) {
    return;
  }

  if (isFileModified() && !isFileEmpty()) {
    QString message = (!isNewFile())
      ? tr("Save changes to %1?").arg(fileNames_.at(idx))
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
    ui_->tabWidget->removeTab(idx);
    editors_.remove(idx);
    fileNames_.remove(idx);
  }

  updateTitle();
}

void MainWindow::on_actionSave_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  if (isNewFile()) {
    on_actionSaveAs_triggered();
    return;
  }

  QFile file(fileNames_[getCurrentView()]);
  if (!file.open(QIODevice::WriteOnly)) {
    QString message =
      tr("Could not save to %1: %2.").arg(fileNames_[getCurrentView()], file.errorString());
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
  settings.setValue("LastFile", fileName);

  fileNames_[getCurrentView()] = fileName;
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
  if (!getCurrentEditor()) {
    return;
  }
  QSettings settings;
  bool useDarkMode = !settings.value("DarkMode", false).toBool();
  settings.setValue("DarkMode", useDarkMode);
  getCurrentEditor()->toggleDarkMode(useDarkMode);
  if (useDarkMode) {
    ui_->splitter->setPalette(darkModePalette);
  } else {
    ui_->splitter->setPalette(defaultPalette);
  }
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

void MainWindow::on_actionCompile_triggered() {
  if (fileNames_.isEmpty()) {
    return;
  }
  if (isNewFile()) {
    on_actionSaveAs_triggered();
  } else {
    on_actionSave_triggered();
  }
  Compiler compiler;
  const QString& fileName = fileNames_[getCurrentView()];
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
  server_.run(fileNames_[getCurrentView()]);
}

void MainWindow::on_actionAbout_triggered() {
  AboutDialog dialog;
  dialog.exec();
}

void MainWindow::on_actionAboutQt_triggered() {
  QMessageBox::aboutQt(this);
}

void MainWindow::on_editor_textChanged() {
  updateTitle();
}

void MainWindow::on_editor_cursorPositionChanged() {
  if (!getCurrentEditor()) {
    return;
  }
  QTextCursor cursor = getCurrentEditor()->textCursor();
  int line = cursor.blockNumber() + 1;
  int column = cursor.columnNumber() + 1;
  dynamic_cast<StatusBar*>(statusBar())->setCursorPosition(line, column);
}

void MainWindow::closeEvent(QCloseEvent *event) {
  on_actionClose_triggered();
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
  return fileNames_[getCurrentView()];
}

EditorWidget* MainWindow::getCurrentEditor() const {
  if (fileNames_.isEmpty()) {
    return 0;
  }
  return editors_[getCurrentView()];
}

int MainWindow::getCurrentView() const {
  return ui_->tabWidget->currentIndex();
}

void MainWindow::updateTitle() {
  QString title;

  if (isNewFile()) {
    title = "Untitled File";
  } else if (fileNames_.isEmpty()) {
    title = "No File";
  } else {
    title = QFileInfo(fileNames_[getCurrentView()]).fileName();
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
  ui_->tabWidget->setCurrentIndex(ui_->tabWidget->count() - 1);
  editors_.last()->setPlainText(file.readAll());
  setFileModified(false);

  return true;
}

bool MainWindow::isNewFile() const {
  return fileNames_.isEmpty() || fileNames_[getCurrentView()].isEmpty();
}

bool MainWindow::isFileModified() const {
  return !editors_.isEmpty() && editors_[getCurrentView()]->document()->isModified();
}

void MainWindow::setFileModified(bool isModified) {
  if (!editors_.isEmpty()) {
    editors_[getCurrentView()]->document()->setModified(isModified);
    updateTitle();
  }
}

bool MainWindow::isFileEmpty() const {
  if (editors_.isEmpty()) {
    return true;
  }
  QTextDocument *document = editors_[getCurrentView()]->document();
  return document->isEmpty()
    || (isNewFile() && !document->toPlainText().contains(QRegExp("\\S")));
}

void MainWindow::createTab(const QString& fileName) {
  QWidget* tab = new QWidget();
  tab->setObjectName(fileName);
  QHBoxLayout* horizontalLayout = new QHBoxLayout(tab);
  horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
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
}

