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

#pragma once

class QWidget;

namespace eka2l1 {
namespace harmattan {
    struct emulator;

    /**
     * @brief Ask the user for a firmware archive/VPL and install it.
     * @returns true when a device was installed.
     */
    bool install_device_dialog(QWidget *parent, emulator &emu);

    /**
     * @brief Ask the user for a SIS/SISX file and install it on the running device.
     * @returns true when a package was installed.
     */
    bool install_package_dialog(QWidget *parent, emulator &emu);
}
}
