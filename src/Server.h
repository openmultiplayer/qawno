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

#ifndef SERVER_H
#define SERVER_H

#include <QString>
#include <QStringList>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Server {
 public:
  Server();
  ~Server();

  QString path() const;
  void setPath(const QString &path);

  QStringList options() const;
  void setOptions(const QString &options);
  void setOptions(const QStringList &options);
  
  QStringList extras() const;
  void setExtras(const QString &extras);
  void setExtras(const QStringList &extras);

  QString output() const;

  QString command() const;
  QString commandFor(const QString &inputFile) const;

  void run(const QString &inputFile);

 private:
  QString path_;
  QStringList options_;
  QStringList extras_;
  QString output_;
  HANDLE thread_;
  PROCESS_INFORMATION pi_;

  static DWORD WINAPI threaded(LPVOID);
};

#endif // SERVER_H
