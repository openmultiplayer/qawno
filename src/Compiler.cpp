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

#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>

#include "Compiler.h"

Compiler::Compiler() {
  QSettings settings;
  path_ = settings.value("CompilerPath", "./pawncc").toString();
  options_ = settings.value("CompilerOptions", "-;+ -(+ -\\ -Z- \"-i%p/%o\" \"-r%p/%o\" \"-i%q/include\" -d3 -t4 \"-o%p/%o\" \"%p/%i\"").toString().split("\\s*");
}

Compiler::~Compiler() {
  QSettings settings;
  settings.setValue("CompilerPath", path_);
  settings.setValue("CompilerOptions", options_.join(" "));
}

QString Compiler::path() const {
  return path_;
}

void Compiler::setPath(const QString &path) {
  path_ = path;
}

QStringList Compiler::options() const {
  return options_;
}

void Compiler::setOptions(const QString &options) {
  options_ = options.split("\\s*");
}

void Compiler::setOptions(const QStringList &options) {
  options_ = options;
}

QString Compiler::output() const {
  return output_;
}

QString Compiler::command() const {
  return QString("%1 %2").arg(path_).arg(options_.join(" "));
}

QString Compiler::commandFor(const QString &inputFile) const {
  QString i = QFileInfo(inputFile).fileName();
  QString p = QFileInfo(inputFile).absolutePath();
  QString o = QFileInfo(inputFile).baseName();

  QString q = QCoreApplication::applicationDirPath();
  QFileInfo cmp = QFileInfo(path_);
  QString c = cmp.isAbsolute() ? cmp.absolutePath() : q + "/" + cmp.path();
  QString d = QDir::currentPath();
  
  // Add the input and output files to the command line.
  // Then replace `-r` with `-rfilename`.
  return QString("%1 %2")
    // Invoke the compiler.
    .arg(path_).arg(options_.join(" "))
    // Custom arguments.
    .replace("%c", c)
    .replace("%q", q)
    .replace("%d", d)
    .replace("%i", i)
    .replace("%o", o)
    .replace("%p", p);
}

void Compiler::run(const QString &inputFile) {
  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.setWorkingDirectory(QDir::currentPath());

  QString command = commandFor(inputFile);
  process.start(command, QStringList(), QProcess::ReadOnly);

  if (process.waitForFinished()) {
    output_ = process.readAll();
  } else {
    output_ = process.errorString();
  }
}
