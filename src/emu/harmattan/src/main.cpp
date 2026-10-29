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

#include <exception>

#include <harmattan/emulator.h>
#include <harmattan/frontend_bridge.h>
#include <harmattan/main_window.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QString>

int main(int argc, char **argv) {
    // Harmattan Qt defaults to the "meego" graphics system, which wraps every
    // widget in EGL and interferes with the frontend's own EGL context on the
    // display widget. Its EGL-failure path also crashes on plain X servers.
    // Widget painting stays CPU-side with raster; the display widget is
    // WA_PaintOnScreen and never painted by Qt anyway.
    QApplication::setGraphicsSystem("raster");

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

    // Last-chance capture of fatal C++ errors into the log file, so device
    // reports show how far the emulator got before dying.
    std::set_terminate([]() {
        if (std::current_exception()) {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception &exc) {
                LOG_CRITICAL(eka2l1::FRONTEND_UI, "std::terminate after exception: {}", exc.what());
            } catch (...) {
                LOG_CRITICAL(eka2l1::FRONTEND_UI, "std::terminate after an unknown exception");
            }
        } else {
            LOG_CRITICAL(eka2l1::FRONTEND_UI, "std::terminate called");
        }

        eka2l1::log::spd_logger->flush();
    });

    LOG_INFO(eka2l1::FRONTEND_UI, "EKA2L1 Harmattan frontend starting");
    LOG_INFO(eka2l1::FRONTEND_UI, "Qt {}.{}.{} ({}), binary at {}, data directory {}",
        QT_VERSION >> 16, (QT_VERSION >> 8) & 0xFF, QT_VERSION & 0xFF, qVersion(),
        application_dir.toStdString().c_str(), data_dir.toStdString().c_str());
    LOG_INFO(eka2l1::FRONTEND_UI, "Runtime resource root: {}", application_dir.toStdString());
    LOG_INFO(eka2l1::FRONTEND_UI, "This session's log file: {}/EKA2L1.log", data_dir.toStdString().c_str());

    eka2l1::harmattan::emulator emu;
    emu.install_dir = application_dir.toStdString();
    LOG_INFO(eka2l1::FRONTEND_UI, "Emulator state object created");

    eka2l1::harmattan::main_window window(emu);
    LOG_INFO(eka2l1::FRONTEND_UI, "Main window constructed");

    eka2l1::harmattan::initialise_frontend_bridge(&window);
    LOG_INFO(eka2l1::FRONTEND_UI, "Dialog bridge installed");

    window.showFullScreen();
    LOG_INFO(eka2l1::FRONTEND_UI, "Main window shown fullscreen");

    window.initialise();
    LOG_INFO(eka2l1::FRONTEND_UI, "Main window initialisation returned");

    const int result = application.exec();

    emu.shutdown();
    eka2l1::harmattan::shutdown_frontend_bridge();
    return result;
}
