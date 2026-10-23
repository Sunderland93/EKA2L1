/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <harmattan/install_dialog.h>

#include <harmattan/emulator.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <package/manager.h>

#include <system/devices.h>
#include <system/epoc.h>
#include <system/installation/archive.h>
#include <system/installation/common.h>
#include <system/installation/firmware.h>

#include <QCoreApplication>
#include <QFileDialog>
#include <QStringList>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include <atomic>

namespace eka2l1 {
namespace harmattan {
    static QString installation_error_to_string(const device_installation_error error) {
        switch (error) {
        case device_installation_none:
            return QString();
        case device_installation_not_exist:
            return QObject::tr("The selected file does not exist.");
        case device_installation_insufficent:
            return QObject::tr("Not enough disk space.");
        case device_installation_rpkg_corrupt:
            return QObject::tr("The RPKG archive is corrupt.");
        case device_installation_determine_product_failure:
            return QObject::tr("Could not determine the device product.");
        case device_installation_already_exist:
            return QObject::tr("This device is already installed.");
        case device_installation_rom_fail_to_copy:
            return QObject::tr("Failed to copy the ROM image.");
        case device_installation_vpl_file_invalid:
            return QObject::tr("The VPL file is invalid.");
        case device_installation_rofs_corrupt:
            return QObject::tr("The ROFS image is corrupt.");
        case device_installation_rom_file_corrupt:
            return QObject::tr("The ROM image is corrupt.");
        case device_installation_fpsx_corrupt:
            return QObject::tr("The FPSX content is corrupt.");
        case device_installation_rpkg_missing:
            return QObject::tr("This ROM needs an RPKG archive to go with it.");
        case device_installation_archive_corrupt:
            return QObject::tr("The archive could not be opened.");
        case device_installation_archive_no_device:
            return QObject::tr("The archive does not contain a device dump.");
        default:
            return QObject::tr("Unknown installation error.");
        }
    }

    bool install_device_dialog(QWidget *parent, emulator &emu) {
        if (!emu.symsys) {
            QMessageBox::critical(parent, QObject::tr("Install device"),
                QObject::tr("The emulator system is not initialised."));
            return false;
        }

        QStringList kinds;
        kinds << QObject::tr("Archive (RPKG, ROM or unpacked dump)")
              << QObject::tr("Firmware VPL");

        bool ok = false;
        const QString kind = QInputDialog::getItem(parent, QObject::tr("Install device"),
            QObject::tr("What kind of file do you have?"), kinds, 0, false, &ok);

        if (!ok) {
            return false;
        }

        const bool use_archive = (kinds.indexOf(kind) == 0);
        const QString filter = use_archive
            ? QObject::tr("Device archives (*.7z *.zip *.tar *.gz *.rar);;All files (*)")
            : QObject::tr("Firmware packages (*.vpl);;All files (*)");

        const QString selected = QFileDialog::getOpenFileName(parent,
            QObject::tr("Select device file"), QString(), filter);

        if (selected.isEmpty()) {
            return false;
        }

        const int isolate_answer = QMessageBox::question(parent, QObject::tr("Install device"),
            QObject::tr("Give this device its own C, D and E drives?\n\n"
                        "Choose Yes unless you are intentionally sharing drives between devices."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        const bool isolate_drives = (isolate_answer == QMessageBox::Yes);

        device_manager *dvc = emu.symsys->get_device_manager();

        QProgressDialog progress(QObject::tr("Installing device..."), QObject::tr("Cancel"), 0, 100, parent);
        progress.setWindowTitle(QObject::tr("Install device"));
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        progress.setValue(0);

        const auto progress_cb = [&progress](const std::size_t taken, const std::size_t total) {
            if (total != 0) {
                progress.setValue(static_cast<int>((taken * 100) / total));
            }
            QCoreApplication::processEvents();
        };

        const auto cancel_cb = [&progress]() {
            return progress.wasCanceled();
        };

        const auto choose_cb = [parent](const std::vector<std::string> &variants) -> int {
            QStringList list;
            for (const std::string &variant : variants) {
                list.append(QString::fromStdString(variant));
            }

            bool chosen_ok = false;
            const QString chosen = QInputDialog::getItem(parent, QObject::tr("Select firmware variant"),
                QObject::tr("Variant:"), list, 0, false, &chosen_ok);

            if (!chosen_ok) {
                return -1;
            }

            const int index = list.indexOf(chosen);
            return (index >= 0) ? index : -1;
        };

        device_installation_error error = device_installation_none;

        if (use_archive) {
            const std::string roms_path = eka2l1::add_path(emu.conf.storage, "roms/");
            const std::string drives_z_path = eka2l1::add_path(emu.conf.storage, "drives/z/");

            error = loader::install_archive(dvc, selected.toStdString(), roms_path, drives_z_path,
                isolate_drives, progress_cb, cancel_cb);
        } else {
            const std::string roms_path = eka2l1::add_path(emu.conf.storage, "roms/");

            error = install_firmware(dvc, selected.toStdString(), emu.conf.storage, roms_path,
                isolate_drives, choose_cb, progress_cb, cancel_cb);
        }

        progress.setValue(100);

        if (progress.wasCanceled()) {
            return false;
        }

        if (error != device_installation_none) {
            QMessageBox::critical(parent, QObject::tr("Installation failed"),
                installation_error_to_string(error));
            return false;
        }

        return true;
    }

    bool install_package_dialog(QWidget *parent, emulator &emu) {
        if (!emu.symsys || !emu.symsys->get_packages()) {
            QMessageBox::information(parent, QObject::tr("Install application"),
                QObject::tr("Boot a device before installing applications."));
            return false;
        }

        const QString file = QFileDialog::getOpenFileName(parent, QObject::tr("Select application"),
            QString(), QObject::tr("Symbian packages (*.sis *.sisx);;All files (*)"));

        if (file.isEmpty()) {
            return false;
        }

        const std::lock_guard<std::recursive_mutex> guard(emu.session_mutex);

        manager::packages *pkgmngr = emu.symsys->get_packages();
        drive_number install_drive = emu.symsys->is_s80_device_active() ? drive_d : drive_e;

        pkgmngr->show_text = [](const char *text, const bool) {
            LOG_INFO(FRONTEND_UI, "Package installer: {}", text ? text : "");
            return true;
        };

        pkgmngr->choose_lang = [](const int *langs, const int count) {
            return (count > 0) ? 0 : -1;
        };

        QProgressDialog progress(QObject::tr("Installing application..."), QObject::tr("Cancel"), 0, 100, parent);
        progress.setWindowTitle(QObject::tr("Install application"));
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        progress.setValue(0);

        const package::installation_result result = pkgmngr->install_package(
            common::utf8_to_ucs2(file.toStdString()), install_drive,
            [&progress](const std::size_t taken, const std::size_t total) {
                if (total != 0) {
                    progress.setValue(static_cast<int>((taken * 100) / total));
                }
                QCoreApplication::processEvents();
            },
            [&progress]() {
                return progress.wasCanceled();
            });

        progress.setValue(100);

        switch (result) {
        case package::installation_result_success:
            return true;

        case package::installation_result_aborted:
            if (!progress.wasCanceled()) {
                QMessageBox::information(parent, QObject::tr("Installation aborted"),
                    QObject::tr("The installation was aborted."));
            }
            return false;

        default:
            QMessageBox::critical(parent, QObject::tr("Installation failed"),
                QObject::tr("Could not install %1. Ensure it is a valid SIS/SISX file.")
                    .arg(file));
            return false;
        }
    }
}
}
