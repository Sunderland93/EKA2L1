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

#include <common/log.h>
#include <common/path.h>

#include <harmattan/emulator.h>
#include <harmattan/frontend_bridge.h>
#include <harmattan/main_window.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QString>

int main(int argc, char **argv) {
    QApplication application(argc, argv);
    application.setApplicationName("EKA2L1");
    application.setOrganizationName("EKA2L1");

    // Shaders, patch DLLs and the default MIDI banks live next to the binary,
    // while config/data/drives stay in the user's home directory.
    const QString application_dir = QCoreApplication::applicationDirPath();
    eka2l1::set_runtime_resource_root(application_dir.toStdString());

    const QString data_dir = QDir::homePath() + "/.eka2l1";
    QDir().mkpath(data_dir);
    QDir::setCurrent(data_dir);

    eka2l1::log::setup_log(nullptr);

    eka2l1::harmattan::emulator emu;
    emu.install_dir = application_dir.toStdString();

    eka2l1::harmattan::main_window window(emu);
    eka2l1::harmattan::initialise_frontend_bridge(&window);
    window.showFullScreen();
    window.initialise();

    const int result = application.exec();

    emu.shutdown();
    eka2l1::harmattan::shutdown_frontend_bridge();
    return result;
}
