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

#include <QPainter>
#include <QSettings>
#include <QTextEdit>
#include <QTextBlock>
#include <QTextCursor>

#include "EditorWidget.h"
#include "SyntaxHighlighter.h"

EditorLineNumberWidget::EditorLineNumberWidget(EditorWidget *editor)
  : QWidget(editor)
{
  connect(editor, SIGNAL(updateRequest(QRect, int)), SLOT(update(QRect, int)));
  connect(editor, SIGNAL(blockCountChanged(int)), SLOT(updateWidth(int)));
  connect(editor, SIGNAL(blockCountChanged(int)), SLOT(updateGeometry()));
}

EditorLineNumberWidget::~EditorLineNumberWidget() {
  // nothing
}

EditorWidget *EditorLineNumberWidget::editor() const {
  return static_cast<EditorWidget*>(parent());
}

QSize EditorLineNumberWidget::sizeHint() const {
  int lineCount = editor()->blockCount();
  int digitWidth = fontMetrics().horizontalAdvance(QLatin1Char('0'));
  int numDigits = QString::number(lineCount).length();
  int width =  digitWidth * (numDigits + 2);
  return QSize(width, 0);
}

void EditorLineNumberWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setPen(palette().windowText().color());
  painter.fillRect(event->rect(), palette().window().color());

  QTextBlock block = editor()->firstVisibleBlock();
  QPointF contentOffset = editor()->contentOffset();
  QRectF boundingGeometry = editor()->blockBoundingGeometry(block);

  qreal top = boundingGeometry.translated(contentOffset).top();
  qreal bottom = top + editor()->blockBoundingRect(block).height();

  do {
    if (bottom >= event->rect().top()) {
      QString lineNumber = QString::number(block.blockNumber() + 1);
      int digitWidth = fontMetrics().horizontalAdvance(QLatin1Char('0'));
      int numDigits = lineNumber.length();
      QRect rect(digitWidth, static_cast<int>(top), digitWidth * numDigits, static_cast<int>(bottom));
      painter.drawText(rect, Qt::AlignRight, lineNumber);
    }
    block = block.next();
    top = bottom;
    bottom = top + editor()->blockBoundingRect(block).height();
  }
  while (block.isValid() && block.isVisible()
         && top <= event->rect().bottom());
}

void EditorLineNumberWidget::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event);
  updateGeometry();
  updateWidth();
}

void EditorLineNumberWidget::update(const QRect &rect, int dy) {
  if (dy != 0) {
    scroll(0, dy);
  } else {
    QWidget::update(0, rect.y(), sizeHint().width(), rect.height());
  }
}

void EditorLineNumberWidget::updateWidth(int) {
  editor()->setViewportMargins(sizeHint().width(), 0, 0, 0);
}

void EditorLineNumberWidget::updateGeometry() {
  QRect cr = editor()->contentsRect();
  setGeometry(cr.left(), cr.top(), sizeHint().width(), cr.height());
}

static QFont defaultFont() {
  #ifdef Q_OS_WIN32
    QFont font("Courier New");
  #else
    QFont font("Monospace");
  #endif
  font.setStyleHint(QFont::Monospace);
  return font;
}

EditorWidget::EditorWidget(QWidget *parent)
  : QPlainTextEdit(parent),
    lineNumberArea_(this),
    highlighter_(this)
{
  QSettings settings;

  QFont font = defaultFont();
  font.fromString(settings.value("EditorFont", font).toString());
  setFont(font);

  setTabWidth(tabWidth_);
  setIndentWidth(indentWidth_);

  setLineWrapMode(NoWrap);
  setUndoRedoEnabled(true);
  setCursorWidth(2);

  QPalette palette;
  palette.setColor(lineNumberArea_.backgroundRole(), Qt::lightGray);
  palette.setColor(lineNumberArea_.foregroundRole(), Qt::black);
  lineNumberArea_.setPalette(palette);

  highlighter_.setDocument(document());

  connect(this, SIGNAL(cursorPositionChanged()), SLOT(highlightCurrentLine()));
  highlightCurrentLine();
}

EditorWidget::~EditorWidget() {
  QSettings settings;
}

int EditorWidget::tabWidth() const {
  return tabWidth_;
}

void EditorWidget::setTabWidth(int width) {
  tabWidth_ = width;
  setTabStopDistance(static_cast<double>(fontMetrics().horizontalAdvance(' ')) * static_cast<double>(width));
}

int EditorWidget::indentWidth() const {
  return indentWidth_;
}

void EditorWidget::setIndentWidth(int width) {
  indentWidth_ = width;
}

void EditorWidget::toggleDarkMode(bool toggle) {
  QPalette lineNumAreaPalette;
  if (toggle) {
    highlighter_.setColorScheme(SyntaxHighlighter::darkModeColorScheme);
    lineNumAreaPalette.setColor(lineNumberArea_.backgroundRole(), QColor(0x2D333D));
    lineNumAreaPalette.setColor(lineNumberArea_.foregroundRole(), QColor(0x636D83));
  } else {
    highlighter_.setColorScheme(SyntaxHighlighter::defaultColorScheme);
    lineNumAreaPalette.setColor(lineNumberArea_.backgroundRole(), Qt::lightGray);
    lineNumAreaPalette.setColor(lineNumberArea_.foregroundRole(), Qt::black);
  }
  usingDarkMode = toggle;
  highlighter_.rehighlight();
  lineNumberArea_.setPalette(lineNumAreaPalette);
  highlightCurrentLine();
}

void EditorWidget::jumpToLine(long line) {
  if (line > 0 && line <= blockCount()) {
    QTextCursor cursor = textCursor();
    int pos = document()->findBlockByLineNumber(line - 1).position();
    cursor.setPosition(pos);
    setTextCursor(cursor);
  }
}

void EditorWidget::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event);
  // FIXME: This should be done somwhere inside EditorLineNumberWidget.
  lineNumberArea_.updateGeometry();
}

void EditorWidget::keyPressEvent(QKeyEvent *event) {
  QTextCursor cursor = textCursor();
  bool removePrevChar = false;

  switch (event->key()) {
    case Qt::Key_Tab:
      if (cursor.hasSelection()) {
        indentSelectedText(cursor);
      } else {
        indentBlock(cursor);
      }
      event->accept();
      return;
    case Qt::Key_Backtab:
      //if (cursor.hasSelection()) {
        unindentSelectedText(cursor);
      //}
      event->accept();
      return;
    case Qt::Key_Enter:
    case Qt::Key_Return: {
      autoIndentOnNewLineInsertion(cursor);
      removePrevChar = true;
      event->accept();
      break;
    }
    case Qt::Key_BraceRight: {
      autoUnindentOnClosingBraceInsertion(cursor);
      event->accept();
      return;
    }
  }

  QPlainTextEdit::keyPressEvent(event);
  if (removePrevChar) {
    textCursor().deletePreviousChar();
  }
}

void EditorWidget::highlightCurrentLine() {
  if (!isReadOnly()) {
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextEdit::ExtraSelection selection;
    if (usingDarkMode) {
      selection.format.setBackground(QColor(0x2D333D));
    } else {
      selection.format.setBackground(QColor(Qt::lightGray).lighter(120));
    }
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    setExtraSelections(extraSelections << selection);
  }
}

void EditorWidget::editSelectedText(QTextCursor cursor,
                             std::function<void(QTextCursor cursor)> callback) {
  int start = cursor.selectionStart();
  int end = cursor.selectionEnd();

  cursor.setPosition(start);
  int startBlock = cursor.blockNumber();

  cursor.setPosition(end);
  int endBlock = cursor.blockNumber();

  cursor.setPosition(start);
  cursor.beginEditBlock();

  for (int i = 0; i <= endBlock - startBlock; i++) {
    cursor.movePosition(QTextCursor::StartOfBlock);
    callback(cursor);
    cursor.movePosition(QTextCursor::NextBlock);
  }

  cursor.endEditBlock();
}

void EditorWidget::indentBlock(QTextCursor cursor) {
  cursor.insertText("\t");
}

void EditorWidget::indentSelectedText(QTextCursor cursor) {
  editSelectedText(cursor, [this](QTextCursor cursor) {
    indentBlock(cursor);
  });
}

void EditorWidget::unindentSelectedText(QTextCursor cursor) {
  editSelectedText(cursor, [this](QTextCursor cursor) {
    int removedChars = 0;
    int blockStart = cursor.position();
    do {
      cursor.setPosition(blockStart + 1, QTextCursor::KeepAnchor);
      QString text = cursor.selectedText();
      if (text == "\t") {
        cursor.removeSelectedText();
        removedChars += tabWidth_;
      } else {
        break;
      }
    } while (removedChars < indentWidth_);
  });
}

void EditorWidget::autoIndentOnNewLineInsertion(QTextCursor cursor) {
  int currPos = cursor.position();
  cursor.setPosition(currPos - 1, QTextCursor::KeepAnchor);
  QString charBefore = cursor.selectedText();
  cursor.setPosition(currPos - cursor.positionInBlock() - 1, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();
  int indents = 0;
  if (charBefore == "{") {
    indents++;
  }
  indents += countIndents(line);
  cursor.setPosition(currPos);
  cursor.insertText("\r\n");
  while (indents) {
    indentBlock(cursor);
    indents--;
  }
}

void EditorWidget::autoUnindentOnClosingBraceInsertion(QTextCursor cursor) {
  int currPos = cursor.position();
  cursor.setPosition(currPos - cursor.positionInBlock(), QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();
  bool allTabs = isLineAllTabs(line);
  if (allTabs) {
    int indents = 0;
    indents += countIndents(line);
    if (indents != 0) {
      indents--;
    }
    while (indents) {
      indentBlock(cursor);
      indents--;
    }
  }
  else {
    cursor.setPosition(currPos);
  }
  cursor.insertText("}");
}

int EditorWidget::countIndents(QString line) {
  int indents = 0;
  for (int i = 0; i < line.length(); i++) {
    if (line[i].toLatin1() == '\t') {
      indents++;
    } else {
      break;
    }
  }
  return indents;
}

bool EditorWidget::isLineAllTabs(QString line) {
  bool allTabs = true;
  for (int i = 0; i < line.length(); i++) {
    if (line[i].toLatin1() != '\t') {
      allTabs = false;
      break;
    }
  }
  return allTabs;
}
