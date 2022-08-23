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

#include "Server.h"

Server::Server() {
  QSettings settings;
  path_ = settings.value("ServerPath", "../omp-server").toString();
  options_ = settings.value("ServerOptions", "--config rcon.password=testing \"%o\"").toString().split("\\s*");
  extras_ = settings.value("ServerExtras", "").toString().split("\\s*");
  ZeroMemory(&pi_, sizeof(PROCESS_INFORMATION));
  thread_ = NULL;
}

Server::~Server() {
  QSettings settings;
  settings.setValue("ServerPath", path_);
  settings.setValue("ServerOptions", options_.join(" "));
  settings.setValue("ServerExtras", extras_.join(" "));
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
  return QString("%1 %2 -- %3").arg(path_).arg(options_.join(" ")).arg(extras_.join(" "));
}

QString Server::commandFor(const QString &inputFile) const {
  QString i = QFileInfo(inputFile).fileName();
  QString p = QFileInfo(inputFile).absolutePath();
  QString o = QFileInfo(inputFile).baseName();

  QString q = QCoreApplication::applicationDirPath();
  QFileInfo cmp = QFileInfo(path_);
  QString c = cmp.isAbsolute() ? cmp.absolutePath() : q + "/" + cmp.path();
  QString d = QDir::currentPath();

  return QString("%1 %2 -- %3")
    // Invoke the compiler.
    .arg(path_).arg(options_.join(" ")).arg(extras_.join(" "))
    // Custom arguments.
    .replace("%c", c)
    .replace("%q", q)
    .replace("%d", d)
    .replace("%i", i)
    .replace("%o", o)
    .replace("%p", p);
}

void Server::run(const QString &inputFile) {
  if (pi_.hProcess) {
    //QString fileName = QFileInfo(inputFile).baseName();
    //std::string cmd = QString("echo %1\r\n").arg(fileName).toStdString();
    //// The process exists.  Send a `changemode` command.
    //DWORD written;
    //WriteFile(stdinW_, cmd.data(), cmd.size(), &written, NULL);
    HANDLE h = pi_.hProcess;
    TerminateThread(thread_, 0);
    CloseHandle(thread_);
    thread_ = NULL;
    TerminateProcess(h, 0);
    CloseHandle(h);
  }
  {
    // Initialise the process.
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.lpTitle = "open.mp server";

    // Spawn a server and wait till it closes to reset some memory.
    std::string cmd = commandFor(inputFile).toStdString();
    std::string path = path_.toStdString();
    QString q = QCoreApplication::applicationDirPath();
    QFileInfo cmp = QFileInfo(path_);
    QString c = cmp.isAbsolute() ? cmp.absolutePath() : q + "/" + cmp.path();
    std::string pwd = c.toStdString();
    CreateProcess(NULL, (char *)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, (char *)pwd.c_str(), &si, &pi_);
    thread_ = CreateThread(NULL, 0, &Server::threaded, &pi_, 0, NULL);
  }
}

DWORD Server::threaded(LPVOID p) {
  HANDLE h = ((PROCESS_INFORMATION*)p)->hProcess;
  WaitForSingleObject(h, INFINITE);
  CloseHandle(h);
  ZeroMemory(p, sizeof(PROCESS_INFORMATION));
  return 0;
}

