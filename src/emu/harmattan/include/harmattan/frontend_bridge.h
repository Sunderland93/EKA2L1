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
    /**
     * @brief Install the drivers::ui dialog backends and the URL opener.
     *
     * The emulator core calls into eka2l1::drivers::ui from the OS thread, so
     * requests are marshalled onto the Qt event loop with queued invocations.
     * Must be called from the UI thread after QApplication exists.
     */
    void initialise_frontend_bridge(QWidget *dialog_parent);

    /**
     * @brief Destroy the bridge. Call after the emulator threads have stopped.
     */
    void shutdown_frontend_bridge();
}
}
