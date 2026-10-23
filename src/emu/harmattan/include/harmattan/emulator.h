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

#include <common/sync.h>
#include <config/app_settings.h>
#include <config/config.h>

#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>
#include <drivers/sensor/sensor.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace eka2l1 {
    class system;
    class window_server;
    class applist_server;

    namespace epoc {
        struct screen;
    }
}

namespace eka2l1 {
namespace harmattan {
    /**
     * @brief Emulator session owned by the Qt4 frontend.
     *
     * The graphics driver lives on its own thread because the EGL context is
     * bound to the thread that created it. The Symbian OS runs on another
     * thread. Access to the guest state from the UI thread is serialized with
     * session_mutex: the OS thread holds it while a loop tick runs, the UI
     * thread takes it while reading or modifying the system.
     */
    struct emulator {
        emulator();
        ~emulator();

        /**
         * @brief Stage one: load config, create the system and mount the device.
         *
         * Must be called from the UI thread. Returns false when no device is
         * installed. The caller may install one and call it again.
         */
        bool boot_ui_side();

        /**
         * @brief Spawn the graphics and OS threads.
         *
         * @param render_window     Native X11 window used by the graphics driver.
         * @param display_connection X11 Display connection (may be nullptr).
         * @param visual_id         X11 visual id of render_window.
         */
        bool start_threads(void *render_window, void *display_connection, std::uint32_t visual_id,
            const int width, const int height);

        /**
         * @brief Stop both worker threads and release the system. Does not join,
         *        call stop_threads() first.
         */
        void shutdown();

        void stop_threads();

        /**
         * @brief Queue a touch/mouse event at window coordinates.
         *
         * @param action 0 = press, 1 = repeat, 2 = release (drivers::mouse_action).
         */
        void send_touch(const int x, const int y, const int button, const int action);

        /**
         * @brief Queue a raw key event with an epoc scan code.
         *
         * Raw events bypass the host keybind map, so the frontend owns the
         * Qt key to scan code translation. Thread-safe.
         */
        void send_key(const int scan_code, const bool pressed);

        void set_surface_size(const int width, const int height);

        bool is_running() const {
            return threads_started_;
        }

        /**
         * @brief Register the screen redraw callback on the focused screen.
         *
         * Safe to call from either the UI or the OS thread.
         */
        void add_screen_draw_callback();

        /**
         * @brief Scan the installed applications. Takes session_mutex.
         */
        void rescan_apps();

        config::state conf;

        std::unique_ptr<config::app_settings> app_settings;
        std::unique_ptr<eka2l1::system> symsys;

        drivers::graphics_driver_ptr graphics_driver;
        drivers::audio_driver_instance audio_driver;
        std::unique_ptr<drivers::sensor_driver> sensor_driver;

        window_server *winserv = nullptr;
        applist_server *applist = nullptr;

        std::recursive_mutex session_mutex;

        std::atomic<bool> should_emu_quit{ false };
        std::atomic<bool> stage_two_inited{ false };
        std::atomic<int> surface_width{ 854 };
        std::atomic<int> surface_height{ 480 };

        // Invoked from the OS thread once stage two has been attempted.
        std::function<void(bool)> boot_finished_callback;

        int present_status = 0;
        std::size_t screen_draw_callback = 0;

        std::string install_dir;

    private:
        friend int os_thread_main(emulator &);
        friend int graphics_thread_main(emulator &);

        drivers::window_system_info wsi_;

        common::event graphics_ready_event_;
        common::event os_done_event_;
        common::event init_event_;

        std::thread os_thread_;
        std::thread graphics_thread_;
        bool threads_started_ = false;

        bool stage_two();
        void on_system_reset(eka2l1::system *the_sys);
    };
}
}
