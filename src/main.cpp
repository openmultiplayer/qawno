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

#include <QApplication>
#include <QCoreApplication>
#include <QTranslator>

#include <qawno.h>
#include "MainWindow.h"

#include <string.h>

class ColourTranslator : public QTranslator
{
public:
  QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override {
    static QChar color[] = { 'c', 'o', 'l', 'o', 'r' };
    static QChar Color[] = { 'C', 'o', 'l', 'o', 'r' };
    static QChar COLOR[] = { 'C', 'O', 'L', 'O', 'R' };
    static QChar colour[] = { 'c', 'o', 'l', 'o', 'u', 'r' };
    static QChar Colour[] = { 'C', 'o', 'l', 'o', 'u', 'r' };
    static QChar COLOUR[] = { 'C', 'O', 'L', 'O', 'U', 'R' };
    return QString(sourceText).replace(color, 5, colour, 6).replace(Color, 5, Colour, 6).replace(COLOR, 5, COLOUR, 6);
  }
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);

  QCoreApplication::setApplicationName("Qawno");
  QCoreApplication::setApplicationVersion(QAWNO_VERSION_STRING);
  QCoreApplication::setOrganizationName("Zeex");
  QCoreApplication::setOrganizationDomain("zeex.github.io");
  QCoreApplication::installTranslator(new ColourTranslator());

  MainWindow mainWindow;
  mainWindow.show();

  return app.exec();
}
