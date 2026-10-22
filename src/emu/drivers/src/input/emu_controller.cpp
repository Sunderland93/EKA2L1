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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/platform.h>
#include <drivers/input/emu_controller.h>
#include <drivers/input/backend/emu_controller_null.h>

#if !EKA2L1_PLATFORM(MEEGO)
#include <drivers/input/backend/emu_controller_sdl2.h>
#endif

namespace eka2l1 {
    namespace drivers {
        emu_controller_ptr new_emu_controller(controller_type ctl_type) {
            switch (ctl_type) {
            case controller_type::null:
                return std::make_unique<emu_controller_null>();
            case controller_type::sdl2:
#if EKA2L1_PLATFORM(MEEGO)
                // Harmattan has no SDL2. The device frontend pushes the input
                // events into the emulator directly.
                return nullptr;
#else
                return std::make_unique<emu_controller_sdl2>();
#endif
            }

            return nullptr;
        }

    }
}
