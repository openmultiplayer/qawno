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
  document()->installEventFilter(this);
}

OutputWidget::~OutputWidget() {
  QSettings settings;
  settings.setValue("OutputFont", font().toString());
}

void OutputWidget::keyPressEvent(QKeyEvent* event) {
  return QPlainTextEdit::keyPressEvent(event);
}


bool OutputWidget::eventFilter(QObject* watched, QEvent* event) {
  switch (event->type()) {
  case QKeyEvent::KeyPress:
    if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Copy)) {
      QTextCursor cursor = textCursor();
      QClipboard* clipboard = QApplication::clipboard();
      QString text = cursor.selectedText();
      clipboard->setText(text);
    }
    switch (static_cast<QKeyEvent*>(event)->key()) {
    case Qt::Key_C:
      if (!(static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier)) {
        break;
      }
      // Manually detect `Ctrl+C` and fall through.
    case Qt::Key_Copy:
    case Qt::Key_Cut: {
      // Or have it passed from the OS.
      QTextCursor cursor = textCursor();
      QClipboard* clipboard = QApplication::clipboard();
      QString text = cursor.selectedText();
      clipboard->setText(text);
      break;
    }
    }
    break;
  }
  // Pass it on.
  return QPlainTextEdit::eventFilter(watched, event);
}

