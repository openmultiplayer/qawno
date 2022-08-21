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

#include <QFileDialog>

#include "ui_ServerSettingsDialog.h"
#include "ServerSettingsDialog.h"

ServerSettingsDialog::ServerSettingsDialog(QWidget *parent)
  : QDialog(parent),
    ui_(new Ui::ServerSettingsDialog)
{
  ui_->setupUi(this);
}

ServerSettingsDialog::~ServerSettingsDialog() {
  delete ui_;
}

QString ServerSettingsDialog::serverPath() const {
  return ui_->serverPath->text();
}

void ServerSettingsDialog::setServerPath(const QString &path) {
  ui_->serverPath->setText(path);
}

QString ServerSettingsDialog::serverOptions() const {
  return ui_->serverOptions->text();
}

void ServerSettingsDialog::setServerOptions(const QString &options) {
  ui_->serverOptions->setText(options);
}

QString ServerSettingsDialog::serverExtras() const {
  return ui_->serverExtras->text();
}

void ServerSettingsDialog::setServerExtras(const QString &extras) {
  ui_->serverExtras->setText(extras);
}

void ServerSettingsDialog::on_browse_clicked() {
  QString path = QFileDialog::getOpenFileName(this,
  #ifdef Q_OS_WIN
    tr("Set server executable"), "pawncc.exe",
    tr("Executables (*.exe)"));
  #else
    tr("Set server executable"), "pawncc",
    tr("All files (*.*)"));
  #endif
  if (!path.isEmpty()) {
    ui_->serverPath->setText(path);
  }
}
