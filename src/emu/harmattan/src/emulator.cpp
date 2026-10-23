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

#include <harmattan/emulator.h>

#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/rgb.h>
#include <common/thread.h>
#include <common/version.h>
#include <common/vecx.h>

#include <drivers/itc.h>

#include <kernel/kernel.h>

#include <package/manager.h>

#include <services/applist/applist.h>
#include <services/window/screen.h>
#include <services/window/window.h>

#include <system/devices.h>
#include <system/epoc.h>

#include <algorithm>

namespace eka2l1 {
namespace harmattan {
    static constexpr const char *GRAPHICS_THREAD_NAME = "Graphics thread";
    static constexpr const char *OS_THREAD_NAME = "Symbian OS thread";

    static bool path_is_absolute(const std::string &path) {
        return !path.empty() && (path[0] == '/');
    }

    static std::string resolve_install_path(const std::string &install_dir, std::string path) {
        if (path_is_absolute(path)) {
            return path;
        }

        return eka2l1::add_path(install_dir, path);
    }

    static void draw_emulator_screen(void *userdata, epoc::screen *scr, const bool is_dsa) {
        emulator *emu = reinterpret_cast<emulator *>(userdata);
        if (!emu || !emu->graphics_driver || !emu->symsys) {
            return;
        }

        emu->graphics_driver->wait_for(&emu->present_status);

        drivers::graphics_command_builder builder;

        const eka2l1::vec2 swapchain_size(emu->surface_width.load(), emu->surface_height.load());
        if ((swapchain_size.x <= 0) || (swapchain_size.y <= 0)) {
            return;
        }

        eka2l1::rect viewport;
        viewport.size = swapchain_size;

        builder.set_swapchain_size(swapchain_size);
        builder.backup_state();

        eka2l1::drivers::filter_option filter = emu->conf.nearest_neighbor_filtering
            ? eka2l1::drivers::filter_option::nearest
            : eka2l1::drivers::filter_option::linear;
        eka2l1::vec4 color_clear = eka2l1::common::rgba_to_vec(emu->conf.display_background_color.load());

        builder.set_feature(eka2l1::drivers::graphics_feature::cull, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::blend, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::clipping, false);
        builder.set_viewport(viewport);

        builder.clear({ color_clear.z / 255.0f, color_clear.y / 255.0f, color_clear.x / 255.0f,
                          color_clear.w / 255.0f, 0.0f, 0.0f },
            eka2l1::drivers::draw_buffer_bit_color_buffer);

        auto &crr_mode = scr->current_mode();
        eka2l1::vec2 size = crr_mode.size;

        if ((scr->ui_rotation % 180) != 0) {
            std::swap(size.x, size.y);
        }

        const float window_width = static_cast<float>(swapchain_size.x);
        const float window_height = static_cast<float>(swapchain_size.y);

        float mult_x = scr->requested_ui_scale_factor > 0.0f
            ? scr->requested_ui_scale_factor
            : (window_width / size.x);
        float mult_y = scr->requested_ui_scale_factor > 0.0f
            ? scr->requested_ui_scale_factor
            : mult_x;

        float width = size.x * mult_x;
        float height = size.y * mult_y;

        if (height > window_height) {
            height = window_height;
            mult_x = mult_y = height / size.y;
            width = size.x * mult_y;
        }
        if (width > window_width) {
            width = window_width;
            mult_x = mult_y = width / size.x;
            height = size.y * mult_x;
        }

        const float x = (window_width - width) / 2.0f;
        const float y = (window_height - height) / 2.0f;

        scr->set_native_scale_factor(emu->graphics_driver.get(), mult_x, mult_y);
        scr->absolute_pos.x = static_cast<int>(x);
        scr->absolute_pos.y = static_cast<int>(y);

        eka2l1::rect dest;
        dest.top = eka2l1::vec2(static_cast<int>(x), static_cast<int>(y));
        dest.size = eka2l1::vec2(static_cast<int>(width), static_cast<int>(height));

        eka2l1::rect src;
        src.size = crr_mode.size;
        src.size *= scr->display_scale_factor;

        eka2l1::drivers::advance_draw_pos_around_origin(dest, scr->ui_rotation);

        if ((scr->ui_rotation % 180) != 0) {
            std::swap(dest.size.x, dest.size.y);
        }

        builder.set_texture_filter(scr->screen_texture, true, filter);
        builder.set_texture_filter(scr->screen_texture, false, filter);
        builder.draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0),
            static_cast<float>(scr->ui_rotation),
            (scr->flags_ & eka2l1::epoc::screen::FLAG_SCREEN_UPSCALE_FACTOR_LOCK)
                ? eka2l1::drivers::bitmap_draw_flag_use_upscale_shader
                : 0);

        builder.load_backup_state();

        emu->present_status = -100;
        builder.present(&emu->present_status);

        eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
        emu->graphics_driver->submit_command_list(retrieved);
    }

    emulator::emulator() {
    }

    emulator::~emulator() {
        shutdown();
    }

    bool emulator::boot_ui_side() {
        conf.deserialize();

        if (log::filterings) {
            log::filterings->parse_filter_string(conf.log_filter);
        }

        LOG_INFO(FRONTEND_UI, "EKA2L1 v0.0.1 ({}-{})", GIT_BRANCH, GIT_COMMIT_HASH);

        conf.sf2_bank_path = resolve_install_path(install_dir, conf.sf2_bank_path);
        conf.hsb_bank_path = resolve_install_path(install_dir, conf.hsb_bank_path);

        app_settings = std::make_unique<config::app_settings>(&conf);

        system_create_components comp;
        comp.audio_ = nullptr;
        comp.graphics_ = nullptr;
        comp.conf_ = &conf;
        comp.settings_ = app_settings.get();

        symsys = std::make_unique<eka2l1::system>(comp);

        device_manager *dvcmngr = symsys->get_device_manager();

        if (dvcmngr->total() > 0) {
            symsys->startup();

            if (!symsys->set_device(conf.device)) {
                LOG_ERROR(FRONTEND_UI, "Failed to set device {}, falling back to device 0", conf.device);
                conf.device = 0;
                symsys->set_device(0);
            }

            symsys->mount_device_drives();
            on_system_reset(symsys.get());
        }

        symsys->add_system_reset_callback([this](eka2l1::system *the_sys) {
            on_system_reset(the_sys);
        });

        stage_two_inited = false;
        return dvcmngr->total() > 0 && dvcmngr->get_current() != nullptr;
    }

    void emulator::on_system_reset(eka2l1::system *the_sys) {
        if (!the_sys) {
            return;
        }

        kernel_system *kern = the_sys->get_kernel_system();
        if (!kern) {
            return;
        }

        winserv = reinterpret_cast<window_server *>(kern->get_by_name<service::server>(
            get_winserv_name_by_epocver(the_sys->get_symbian_version_use())));

        applist = reinterpret_cast<applist_server *>(kern->get_by_name<service::server>(
            get_app_list_server_name_by_epocver(the_sys->get_symbian_version_use())));

        if (stage_two_inited) {
            the_sys->initialize_user_parties();
        }

        if (winserv) {
            add_screen_draw_callback();
        }
    }

    void emulator::add_screen_draw_callback() {
        const std::lock_guard<std::recursive_mutex> guard(session_mutex);

        if (!winserv || (screen_draw_callback != 0)) {
            return;
        }

        epoc::screen *scr = winserv->get_current_focus_screen();
        if (!scr) {
            return;
        }

        screen_draw_callback = scr->add_screen_redraw_callback(this, draw_emulator_screen);
    }

    bool emulator::stage_two() {
        if (stage_two_inited) {
            return true;
        }

        device_manager *dvcmngr = symsys->get_device_manager();
        device *dvc = dvcmngr->get_current();

        if (!dvc) {
            LOG_ERROR(FRONTEND_UI, "No current device is available. Stage two aborted");
            return false;
        }

        LOG_INFO(FRONTEND_UI, "Booting device: {} ({})", dvc->model, dvc->firmware_code);

        symsys->mount(drive_z, drive_media::rom,
            eka2l1::add_path(conf.storage, "/drives/z/"),
            io_attrib_internal | io_attrib_write_protected);

        drivers::player_type player_be = drivers::player_type_tsf;
        if (conf.midi_backend == config::MIDI_BACKEND_MINIBAE) {
            player_be = drivers::player_type_minibae;
        }

        audio_driver = drivers::make_audio_driver(drivers::audio_driver_backend::cubeb,
            conf.audio_master_volume, player_be);

        if (audio_driver) {
            audio_driver->set_bank_path(drivers::MIDI_BANK_TYPE_HSB, conf.hsb_bank_path);
            audio_driver->set_bank_path(drivers::MIDI_BANK_TYPE_SF2, conf.sf2_bank_path);
        }

        symsys->set_audio_driver(audio_driver.get());

        sensor_driver = drivers::sensor_driver::instantiate();
        if (!sensor_driver) {
            LOG_WARN(FRONTEND_UI, "Failed to create sensor driver");
        }
        symsys->set_sensor_driver(sensor_driver.get());

        symsys->initialize_user_parties();

        io_system *io = symsys->get_io_system();

        if (!conf.svg_icon_cache_reset) {
            common::delete_folder("cache\\");

            conf.svg_icon_cache_reset = true;
            conf.serialize(false);
        }

        std::vector<std::tuple<std::u16string, std::string, epocver>> dlls_need_to_copy = {
            { u"Z:\\sys\\bin\\goommonitor.dll", runtime_resource_path("patch/goommonitor_general.dll"), epocver::epoc94 },
            { u"Z:\\sys\\bin\\avkonfep.dll", runtime_resource_path("patch/avkonfep_general.dll"), epocver::epoc93fp1 }
        };

        for (std::size_t i = 0; i < dlls_need_to_copy.size(); i++) {
            epocver ver_required = std::get<2>(dlls_need_to_copy[i]);
            if (symsys->get_symbian_version_use() < ver_required) {
                continue;
            }

            std::u16string org_file_path = std::get<0>(dlls_need_to_copy[i]);
            auto where_to_copy = io->get_raw_path(org_file_path);

            if (!where_to_copy.has_value()) {
                continue;
            }

            std::string where_to_copy_u8 = common::ucs2_to_utf8(where_to_copy.value());
            std::string where_to_backup_u8 = where_to_copy_u8 + ".bak";

            if (common::exists(where_to_copy_u8) && !common::exists(where_to_backup_u8)) {
                common::move_file(where_to_copy_u8, where_to_backup_u8);
            }

            common::copy_file(std::get<1>(dlls_need_to_copy[i]), where_to_copy_u8, true);
        }

        manager::packages *pkgmngr = symsys->get_packages();
        pkgmngr->load_registries();
        pkgmngr->migrate_legacy_registries();

        stage_two_inited = true;
        return true;
    }

    void emulator::set_surface_size(const int width, const int height) {
        if ((width <= 0) || (height <= 0)) {
            return;
        }

        surface_width = width;
        surface_height = height;

        if (graphics_driver) {
            graphics_driver->update_surface_size(eka2l1::vec2(width, height));
        }
    }

    void emulator::send_touch(const int x, const int y, const int button, const int action) {
        if (!symsys) {
            return;
        }

        const std::lock_guard<std::recursive_mutex> guard(session_mutex);
        if (!winserv) {
            return;
        }

        float scale = conf.ui_scale;
        if (scale <= 0.0f) {
            scale = 1.0f;
        }

        drivers::input_event evt;
        evt.type_ = drivers::input_event_type::touch;
        evt.mouse_.raw_screen_pos_ = false;
        evt.mouse_.pos_x_ = static_cast<int>(x / scale);
        evt.mouse_.pos_y_ = static_cast<int>(y / scale);
        evt.mouse_.pos_z_ = PRESSURE_MAX_NUM;
        evt.mouse_.mouse_id = 0;
        evt.mouse_.button_ = static_cast<drivers::mouse_button>(button);
        evt.mouse_.action_ = static_cast<drivers::mouse_action>(action);

        winserv->queue_input_from_driver(evt);
    }

    void emulator::send_key(const int scan_code, const bool pressed) {
        if (!symsys) {
            return;
        }

        const std::lock_guard<std::recursive_mutex> guard(session_mutex);
        if (!winserv) {
            return;
        }

        drivers::input_event evt;
        evt.type_ = drivers::input_event_type::key_raw;
        evt.key_.code_ = scan_code;
        evt.key_.state_ = pressed ? drivers::key_state::pressed : drivers::key_state::released;

        winserv->queue_input_from_driver(evt);
    }

    void emulator::rescan_apps() {
        if (!symsys) {
            return;
        }

        const std::lock_guard<std::recursive_mutex> guard(session_mutex);

        kernel_system *kern = symsys->get_kernel_system();
        if (!kern) {
            return;
        }

        applist = reinterpret_cast<applist_server *>(kern->get_by_name<service::server>(
            get_app_list_server_name_by_epocver(kern->get_epoc_version())));

        if (applist) {
            applist->rescan_registries(symsys->get_io_system());
        }
    }

    int graphics_thread_main(emulator &emu) {
        common::set_thread_name(GRAPHICS_THREAD_NAME);
        common::set_thread_priority(common::thread_priority_high);

        emu.graphics_driver = drivers::create_graphics_driver(drivers::graphic_api::opengl, emu.wsi_);

        if (!emu.graphics_driver) {
            LOG_CRITICAL(FRONTEND_UI, "Failed to create the OpenGL ES graphics driver");
            emu.should_emu_quit = true;
            emu.graphics_ready_event_.set();
            return 1;
        }

        emu.graphics_driver->update_surface_size(eka2l1::vec2(emu.wsi_.surface_width, emu.wsi_.surface_height));
        emu.graphics_driver->set_display_hook([]() {});

        emu.symsys->set_graphics_driver(emu.graphics_driver.get());
        emu.graphics_ready_event_.set();

        emu.graphics_driver->run();

        // The system is torn down by the OS thread; the EGL context must
        // outlive it because the reset path may still submit commands.
        emu.os_done_event_.wait();
        emu.graphics_driver.reset();

        return 0;
    }

    int os_thread_main(emulator &emu) {
        common::set_thread_name(OS_THREAD_NAME);
        common::set_thread_priority(common::thread_priority_high);

        bool inited = false;

        while (!emu.should_emu_quit) {
            if (!inited) {
                inited = emu.stage_two();

                if (emu.boot_finished_callback) {
                    emu.boot_finished_callback(inited);
                }

                if (!inited) {
                    emu.init_event_.wait();
                    continue;
                }
            }

            try {
                const std::lock_guard<std::recursive_mutex> guard(emu.session_mutex);
                emu.symsys->loop();
            } catch (const std::exception &exc) {
                LOG_ERROR(FRONTEND_UI, "Emulator loop exited with an exception: {}", exc.what());
                emu.should_emu_quit = true;
                break;
            }
        }

        {
            const std::lock_guard<std::recursive_mutex> guard(emu.session_mutex);
            emu.winserv = nullptr;
            emu.applist = nullptr;
            emu.screen_draw_callback = 0;
            emu.symsys.reset();
        }

        emu.os_done_event_.set();
        return 0;
    }

    bool emulator::start_threads(void *render_window, void *display_connection, std::uint32_t visual_id,
        const int width, const int height) {
        if (threads_started_) {
            return true;
        }

        graphics_ready_event_.reset();
        os_done_event_.reset();
        init_event_.reset();

        should_emu_quit = false;
        stage_two_inited = false;

        wsi_ = drivers::window_system_info(drivers::window_system_type::harmattan, display_connection,
            render_window, render_window);
        wsi_.native_visual_id = visual_id;
        wsi_.surface_width = static_cast<std::uint32_t>(width);
        wsi_.surface_height = static_cast<std::uint32_t>(height);
        wsi_.render_surface_scale = 1.0f;

        surface_width = width;
        surface_height = height;

        graphics_thread_ = std::thread(graphics_thread_main, std::ref(*this));
        threads_started_ = true;

        if (!graphics_ready_event_.wait_for(10 * 1000 * 1000)) {
            LOG_CRITICAL(FRONTEND_UI, "Timed out waiting for the graphics driver to initialise");
            stop_threads();
            return false;
        }

        if (should_emu_quit) {
            stop_threads();
            return false;
        }

        os_thread_ = std::thread(os_thread_main, std::ref(*this));
        return true;
    }

    void emulator::stop_threads() {
        if (!threads_started_) {
            return;
        }

        should_emu_quit = true;

        if (symsys) {
            kernel_system *kern = symsys->get_kernel_system();
            if (kern) {
                kern->stop_cores_idling();
            }
        }

        if (graphics_driver) {
            graphics_driver->abort();
        }

        init_event_.set();

        if (os_thread_.joinable()) {
            os_thread_.join();
        }

        os_done_event_.set();

        if (graphics_thread_.joinable()) {
            graphics_thread_.join();
        }

        threads_started_ = false;
    }

    void emulator::shutdown() {
        stop_threads();

        const std::lock_guard<std::recursive_mutex> guard(session_mutex);

        winserv = nullptr;
        applist = nullptr;
        screen_draw_callback = 0;

        symsys.reset();
        sensor_driver.reset();
        audio_driver.reset();
        app_settings.reset();
        stage_two_inited = false;
    }
}
}
