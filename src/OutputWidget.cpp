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

#include <QSettings>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QFileInfo>

#include "OutputWidget.h"

static QFont defaultFont() {
  #ifdef Q_OS_WINDOWS
    QFont font("Courier New");
  #else
    QFont font("Monospace");
  #endif
  font.setStyleHint(QFont::Monospace);
  return font;
}

OutputWidget::OutputWidget(QWidget *parent):
  QPlainTextEdit(parent)
{
  QSettings settings;
  QFont font = defaultFont();
  font.fromString(settings.value("OutputFont", font).toString());
  setFont(font);
}

OutputWidget::~OutputWidget() {
  QSettings settings;
  settings.setValue("OutputFont", font().toString());
}

void OutputWidget::keyPressEvent(QKeyEvent* event) {
  if (event->matches(QKeySequence::SelectAll)) {
    QTextCursor cursor = textCursor();
    cursor.select(QTextCursor::Document);
    setTextCursor(cursor);
  } else if (event->matches(QKeySequence::Copy)) {
    QTextCursor cursor = textCursor();
    QApplication::clipboard()->setText(cursor.selectedText().replace(QChar::ParagraphSeparator, '\n'));
  } else switch (event->key()) {
  case Qt::Key_C:
  case Qt::Key_X:
    if (!(event->modifiers() & Qt::ControlModifier)) {
      break;
    }
    // Manually detect `Ctrl+C` and fall through.
  case Qt::Key_Copy:
  case Qt::Key_Cut: {
    // Or have it passed from the OS.
    QApplication::clipboard()->setText(textCursor().selectedText().replace(QChar::ParagraphSeparator, '\n'));
    break;
  }
  case Qt::Key_A:
    if (event->modifiers() & Qt::ControlModifier) {
      QTextCursor cursor = textCursor();
      cursor.select(QTextCursor::Document);
      setTextCursor(cursor);
    }
    break;
  }
  return;
}

void OutputWidget::resetErrorCounter() {
  error_ = -1;
}

OutputWidget::error_selection_s OutputWidget::advanceErrorCounter() {
  // Every time this is called we select different bits of the error text.
  if (error_ != -1) {
    // Find the Nth error/warning.
    QTextCursor cursor = textCursor();
    cursor.setPosition(0);
    int err = -1;
    // Example:
    //
    //   D:\open.mp\gamemodes\independence.pwn(9) : warning 203: symbol is never used: "warning"
    //
    QRegularExpression message("^(.*?)\\((\\d+)\\)");
    do {
      cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      QString text = cursor.selectedText();
      QRegularExpressionMatch match = message.match(text);
      if (match.hasMatch()) {
        ++err;
        // Loop through the errors.
        if (err == error_) {
          QString fileName = match.captured(1);
          QString line = match.captured(2);
          // Normalise the filename so we can compare it to open tabs.
          QFileInfo info(fileName);
          setTextCursor(cursor);
          ++error_;
          return { info.absoluteFilePath(), line.toInt() };
        }
      }
    } while (cursor.movePosition(QTextCursor::NextBlock));
  }
  // Select and copy everything.
  QTextCursor cursor = textCursor();
  cursor.select(QTextCursor::Document);
  QApplication::clipboard()->setText(cursor.selectedText().replace(QChar::ParagraphSeparator, '\n'));
  setTextCursor(cursor);
  error_ = 0;
  return { "", -1 };
}

