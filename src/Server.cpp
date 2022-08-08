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

#include "Server.h"

Server::Server() {
  QSettings settings;
  path_ = settings.value("ServerPath", "../omp-server").toString();
  options_ = settings.value("ServerOptions", "").toString().split("\\s*");
}

Server::~Server() {
  QSettings settings;
  settings.setValue("ServerPath", path_);
  settings.setValue("ServerOptions", options_.join(" "));
}

QString Server::path() const {
  return path_;
}

void Server::setPath(const QString &path) {
  path_ = path;
}

QStringList Server::options() const {
  return options_;
}

void Server::setOptions(const QString &options) {
  options_ = options.split("\\s*");
}

void Server::setOptions(const QStringList &options) {
  options_ = options;
}

QStringList Server::extras() const {
  return extras_;
}

void Server::setExtras(const QString &extras) {
  extras_ = extras.split("\\s*");
}

void Server::setExtras(const QStringList &extras) {
  extras_ = extras;
}

QString Server::output() const {
  return output_;
}

QString Server::command() const {
  return QString("%1 %2 -- %s").arg(path_).arg(options_.join(" ")).arg(extras_.join(" "));
}

QString Server::commandFor(const QString &inputFile) const {
  QString fileName = QFileInfo(inputFile).baseName();
  return QString("%1 %2 %3 -- %4").arg(path_).arg(options_.join(" ")).arg(fileName).arg(extras_.join(" "));
}

void Server::run(const QString &inputFile) {
  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.setWorkingDirectory(QFileInfo(inputFile).absolutePath());

  QString command = commandFor(inputFile);
  process.start(command, QStringList(), QProcess::ReadOnly);

  if (process.waitForFinished()) {
    output_ = process.readAll();
  } else {
    output_ = process.errorString();
  }
}
