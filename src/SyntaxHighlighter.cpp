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

#include "SyntaxHighlighter.h"

SyntaxHighlighter::ColorScheme SyntaxHighlighter::defaultColorScheme = {
  Qt::darkBlue,
  Qt::blue,
  Qt::darkGreen,
  Qt::darkGreen,
  Qt::black,
  Qt::darkMagenta,
  Qt::darkMagenta,
  Qt::darkRed,
  QColor(0x906040)
};

SyntaxHighlighter::ColorScheme SyntaxHighlighter::darkModeColorScheme = {
  QColor(0xABB2BF),
  QColor(0x10B1FE),
  QColor(0x636D83),
  QColor(0x636D83),
  QColor(0xCCCCCC),
  QColor(0xFF78F8),
  QColor(0xFF78F8),
  QColor(0xF9C859),
  QColor(0x9F71CA)
};

SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
  : QSyntaxHighlighter(parent)
{
  colorScheme_ = defaultColorScheme;

  keywords_
    << "@"
    << "@foreign"
    << "@global"
    << "@hook"
    << "@ptask"
    << "@remote"
    << "@return"
    << "@task"
    << "@test"
    << "@timer"
    << "_"
    << "__addressof"
    << "__emit"
    << "__nameof"
    << "__pragma"
    << "assert"
    << "break"
    << "case"
    << "char"
    << "const"
    << "continue"
    << "decl"
    << "default"
    << "defer"
    << "defined"
    << "do"
    << "else"
    << "enum"
    << "exit"
    << "false"
    << "final"
    << "for"
    << "foreach"
    << "foreign"
    << "forward"
    << "global"
    << "goto"
    << "if"
    << "inline"
    << "hook"
    << "native"
    << "new"
    << "operator"
    << "ptask"
    << "public"
    << "repeat"
    << "return"
    << "sizeof"
    << "sleep"
    << "state"
    << "static"
    << "stock"
    << "stop"
    << "switch"
    << "tagof"
    << "task"
    << "timer"
    << "true"
    << "using"
    << "while"
    << "yield";
}

SyntaxHighlighter::~SyntaxHighlighter() {
  // nothing
}

const SyntaxHighlighter::ColorScheme &SyntaxHighlighter::colorScheme() const {
  return colorScheme_;
}

void SyntaxHighlighter::setColorScheme(const ColorScheme &scheme) {
  colorScheme_ = scheme;
}

bool SyntaxHighlighter::isIdentifierFirstChar(QChar c) {
  return c.isLetter() || c == '_' || c == '@';
}

bool SyntaxHighlighter::isIdentifierChar(QChar c) {
  return isIdentifierFirstChar(c) || c.isDigit();
}

bool SyntaxHighlighter::isHexDigit(QChar c) {
  if (c.isDigit()) {
    return true;
  }
  char cc = c.toLatin1();
  return (cc >= 'a' && cc <= 'f') || (cc >= 'A' && cc <= 'F') || cc == 'x';
}

bool SyntaxHighlighter::isKeyword(const QString &s) {
  return keywords_.contains(s);
}

void SyntaxHighlighter::highlightBlock(const QString &text) {
  setFormat(0, text.length(), colorScheme_.defaultColor);

  enum State {
    Unknown = -1,
    CommentBegin,
    Comment,
    CommentEnd,
    Identifier,
    NumericLiteral,
    CharacterLiteral,
    StringLiteral,
    Preprocessor,
    PreprocessorNextLine
  };

  State state = (State)previousBlockState();

  for (int i = 0; i < text.length(); ++i) {
    switch (state) {
    case Comment:
      if (text[i] == '*') {
        state = CommentEnd;
      }
      setFormat(i, 1, colorScheme_.cComment);
      break;
    case CommentBegin:
      if (text[i] == '/') {
        setFormat(i - 1, text.length() - i + 1, colorScheme_.cppComment);
        goto end;
      } else if (text[i] == '*') {
        setFormat(i - 1, 2, colorScheme_.cComment);
        state = Comment;
      } else {
        state = Unknown;
      }
      break;
    case CommentEnd:
      setFormat(i, 1, colorScheme_.cComment);
      if (text[i] == '/')
      {
        state = Unknown;
      } else if (text[i] == '*') {
        state = CommentEnd;
      } else {
        state = Comment;
      }
      break;
    case Identifier:
      if (isIdentifierChar(text[i])) {
        setFormat(i, 1, colorScheme_.identifier);
      } else {
        --i;
        QString ident;
        int start = i;
        while (start >= 0 && isIdentifierChar(text[start])) {
          ident.prepend(text[start--]);
        }
        if (isKeyword(ident)) {
          setFormat(start + 1, ident.length(), colorScheme_.keyword);
        }
        state = Unknown;
      }
      break;
    case NumericLiteral:
      if (text[i].isDigit() || isHexDigit(text[i]) || text[i] == '.' || text[i] == '_') {
        setFormat(i, 1, colorScheme_.number);
      } else {
        --i;
        state = Unknown;
      }
      break;
    case CharacterLiteral:
      setFormat(i, 1, colorScheme_.character);
      if (text[i] == '\'') {
        state = Unknown;
      }
      break;
    case StringLiteral:
      setFormat(i, 1, colorScheme_.string);
      if (text[i] == '\"') {
        state = Unknown;
      }
      break;
    case Preprocessor:
      if (text[i] == '\\') {
        state = PreprocessorNextLine;
        goto end;
      } else {
        if (text[i].isSpace()) {
          state = Unknown;
        } else {
          setFormat(i, 1, colorScheme_.preprocessor);
        }
      }
      break;
    case PreprocessorNextLine:
      break;
    default:
      if (text[i] == '\'') {
        state = CharacterLiteral;
        setFormat(i, 1, colorScheme_.character);
      } else if (isIdentifierFirstChar(text[i])) {
        state = Identifier;
        setFormat(i, 1, colorScheme_.identifier);
      } else if (text[i].isDigit()) {
        setFormat(i, 1, colorScheme_.number);
        state = NumericLiteral;
      } else if (text[i] == '/') {
        state = CommentBegin;
      } else if (text[i] == '\"') {
        state = StringLiteral;
        setFormat(i, 1, colorScheme_.string);
      } else if (text[i] == '#') {
        state = Preprocessor;
        setFormat(i, 1, colorScheme_.preprocessor);
      } else if (text[i] == '<') {
        if (text.contains("#include") || text.contains("#tryinclude")) {
          int count = 0;
          while (count < text.length()) {
            ++count;
          }
          setFormat(i, count, colorScheme_.string);
          i += count;
        }
      }
      break;
    }
  }

end:
  // Some styles automatically end at the end of a line.
  switch (state) {
  case Identifier: {
    QString ident;
    int start = text.length() - 1;
    while (start >= 0 && isIdentifierChar(text[start])) {
      ident.prepend(text[start--]);
    }
    if (isKeyword(ident)) {
      setFormat(start + 1, ident.length(), colorScheme_.keyword);
    }
    setCurrentBlockState((int)Unknown);
    break;
  }
  case CommentBegin:
  case NumericLiteral:
  case Preprocessor:
  case CharacterLiteral:
    setCurrentBlockState((int)Unknown);
    break;
  case Unknown:
  case Comment:
  case CommentEnd:
  case StringLiteral:
  case PreprocessorNextLine:
    setCurrentBlockState((int)state);
    break;
  }
}
