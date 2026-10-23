/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include "context_egl.h"
#include <common/log.h>

#include <vector>

namespace eka2l1::drivers::graphics {
    static constexpr std::array<EGLint, 13> egl_attribs{ EGL_SURFACE_TYPE,
                                                         EGL_WINDOW_BIT,
                                                         EGL_BLUE_SIZE,
                                                         8,
                                                         EGL_GREEN_SIZE,
                                                         8,
                                                         EGL_RED_SIZE,
                                                         8,
                                                         EGL_DEPTH_SIZE,
                                                         0,
                                                         EGL_STENCIL_SIZE,
                                                         0,
                                                         EGL_NONE };
    static constexpr std::array<EGLint, 15> egl_attribs_es{ EGL_SURFACE_TYPE,
                                                         EGL_WINDOW_BIT,
                                                         EGL_RENDERABLE_TYPE,
                                                         EKA2L1_EGL_RENDERABLE_TYPE,
                                                         EGL_BLUE_SIZE,
                                                         8,
                                                         EGL_GREEN_SIZE,
                                                         8,
                                                         EGL_RED_SIZE,
                                                         8,
                                                         EGL_DEPTH_SIZE,
                                                         0,
                                                         EGL_STENCIL_SIZE,
                                                         0,
                                                         EGL_NONE };
    // On Harmattan the config picked here is later used with
    // eglCreateWindowSurface, so it has to advertise window support.
#if EKA2L1_PLATFORM(MEEGO)
    static constexpr EGLint egl_fake_surface_type_es = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
#else
    static constexpr EGLint egl_fake_surface_type_es = EGL_PBUFFER_BIT;
#endif
    static constexpr std::array<EGLint, 15> egl_fake_attribs_es{ EGL_SURFACE_TYPE,
                                                              egl_fake_surface_type_es,
                                                              EGL_RENDERABLE_TYPE,
                                                              EKA2L1_EGL_RENDERABLE_TYPE,
                                                              EGL_BLUE_SIZE,
                                                              8,
                                                              EGL_GREEN_SIZE,
                                                              8,
                                                              EGL_RED_SIZE,
                                                              8,
                                                              EGL_DEPTH_SIZE,
                                                              0,
                                                              EGL_STENCIL_SIZE,
                                                              0,
                                                              EGL_NONE };
    static constexpr std::array<EGLint, 13> egl_fake_attribs{ EGL_SURFACE_TYPE,
                                                              EGL_PBUFFER_BIT,
                                                              EGL_BLUE_SIZE,
                                                              8,
                                                              EGL_GREEN_SIZE,
                                                              8,
                                                              EGL_RED_SIZE,
                                                              8,
                                                              EGL_DEPTH_SIZE,
                                                              0,
                                                              EGL_STENCIL_SIZE,
                                                              0,
                                                              EGL_NONE };
    static constexpr EGLint egl_pbuffer_attribs[5]{ EGL_WIDTH,
                                                    10,
                                                    EGL_HEIGHT,
                                                    10,
                                                    EGL_NONE };
    static constexpr std::array<EGLint, 5> egl_empty_attribs{ EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    static constexpr std::array<EGLint, 3> egl_context_attribs_es{ EGL_CONTEXT_CLIENT_VERSION, EKA2L1_EGL_CONTEXT_CLIENT_VERSION, EGL_NONE };

    bool gl_context_egl::is_headless() const {
        return !render_window;
    }

    bool gl_context_egl::create_context(EGLContext shared, std::pair<int, int> *target_version) {
        if (m_opengl_mode == mode::opengl_es) {
            if (egl_context = eglCreateContext(egl_display, egl_config, shared, egl_context_attribs_es.data());
                    egl_context == EGL_NO_CONTEXT) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreateContext() failed");
                return false;
            }
        } else {
            std::array<EGLint, 5> egl_context_attrib_template{ EGL_CONTEXT_MAJOR_VERSION, 0,
                EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE };
            
            if (target_version != nullptr) {
                egl_context_attrib_template[1] = target_version->first;
                egl_context_attrib_template[3] = target_version->second;

                if (egl_context = eglCreateContext(egl_display, egl_config, shared, egl_context_attrib_template.data());
                        egl_context == EGL_NO_CONTEXT) {
                    LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreateContext() failed");
                    return false;
                }

                context_version = *target_version;
            } else {
                for (auto version: s_desktop_opengl_versions) {
                    egl_context_attrib_template[1] = version.first;
                    egl_context_attrib_template[3] = version.second;

                    if (egl_context = eglCreateContext(egl_display, egl_config, shared, egl_context_attrib_template.data());
                            egl_context != EGL_NO_CONTEXT) {
                        context_version = version;
                        break;
                    }
                }

                if (egl_context == EGL_NO_CONTEXT) {
                    LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreateContext() failed");
                    return false;
                }
            }
        }

        return true;
    }

#if EKA2L1_PLATFORM(MEEGO)
    bool gl_context_egl::choose_window_config() {
        // The window was already created by the frontend with a concrete X11
        // visual, so a configuration whose EGL_NATIVE_VISUAL_ID differs from
        // it would make eglCreateWindowSurface fail with EGL_BAD_MATCH.
        // eglChooseConfig has no way to ask for a visual id, so enumerate.
        EGLint config_count = 0;
        if (eglGetConfigs(egl_display, nullptr, 0, &config_count) != EGL_TRUE || (config_count <= 0)) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglGetConfigs() returned no configurations");
            return false;
        }

        std::vector<EGLConfig> configs(static_cast<std::size_t>(config_count));
        if (eglGetConfigs(egl_display, configs.data(), config_count, &config_count) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglGetConfigs() failed");
            return false;
        }

        EGLConfig fallback = nullptr;

        for (EGLint i = 0; i < config_count; i++) {
            EGLint surface_type = 0;
            EGLint renderable_type = 0;
            EGLint red_size = 0;
            EGLint green_size = 0;
            EGLint blue_size = 0;

            if (eglGetConfigAttrib(egl_display, configs[i], EGL_SURFACE_TYPE, &surface_type) != EGL_TRUE ||
                eglGetConfigAttrib(egl_display, configs[i], EGL_RENDERABLE_TYPE, &renderable_type) != EGL_TRUE ||
                eglGetConfigAttrib(egl_display, configs[i], EGL_RED_SIZE, &red_size) != EGL_TRUE ||
                eglGetConfigAttrib(egl_display, configs[i], EGL_GREEN_SIZE, &green_size) != EGL_TRUE ||
                eglGetConfigAttrib(egl_display, configs[i], EGL_BLUE_SIZE, &blue_size) != EGL_TRUE) {
                continue;
            }

            if (!(surface_type & EGL_WINDOW_BIT) || !(renderable_type & EKA2L1_EGL_RENDERABLE_TYPE)) {
                continue;
            }
            if ((red_size < 5) || (green_size < 5) || (blue_size < 5)) {
                continue;
            }

            if (!fallback) {
                fallback = configs[i];
            }

            if (wsi.native_visual_id == 0) {
                continue;
            }

            EGLint visual_id = 0;
            if (eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &visual_id) == EGL_TRUE &&
                (static_cast<std::uint32_t>(visual_id) == wsi.native_visual_id)) {
                egl_config = configs[i];
                return true;
            }
        }

        if (fallback) {
            LOG_WARN(DRIVER_GRAPHICS, "No EGL config matches X11 visual 0x{:X}, using a fallback",
                wsi.native_visual_id);
            egl_config = fallback;
            return true;
        }

        LOG_CRITICAL(DRIVER_GRAPHICS, "No EGL window configuration is usable for OpenGL ES 2");
        return false;
    }
#endif

    void gl_context_egl::init_gl() {
        if (egl_display = eglGetDisplay(wsi.display_connection ? reinterpret_cast<EGLNativeDisplayType>(wsi.display_connection) : EGL_DEFAULT_DISPLAY); egl_display == EGL_NO_DISPLAY) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglGetDisplay() failed");
            return;
        }
        if (eglInitialize(egl_display, 0, 0) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglInitialize() failed");
            return;
        }

        if (eglBindAPI((m_opengl_mode == mode::opengl_es) ? EGL_OPENGL_ES_API : EGL_OPENGL_API) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "Can't bind target graphics API!");
            return;
        }

        if (m_opengl_mode == mode::opengl_es) {
#if EKA2L1_PLATFORM(MEEGO)
            // The frontend owns a native window and hands it over at driver
            // creation time, so the context can be bound to a real window
            // surface right away. This avoids rendering into a pbuffer that is
            // never presented and keeps the EGL config in sync with the X11
            // visual the window was created with.
            if (wsi.render_window) {
                if (!choose_window_config()) {
                    return;
                }

                render_window = wsi.render_window;
                create_surface();

                if (egl_surface == EGL_NO_SURFACE) {
                    return;
                }

                if (!create_context(nullptr, nullptr)) {
                    return;
                }

                if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
                    LOG_CRITICAL(DRIVER_GRAPHICS, "eglMakeCurrent() failed");
                    return;
                }

                return;
            }
#endif
            if (EGLint egl_num_configs{}; eglChooseConfig(egl_display, egl_fake_attribs_es.data(), &egl_config, 1,
                                &egl_num_configs) != EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglChooseConfig() failed");
                return;
            }
            if (egl_surface = eglCreatePbufferSurface(egl_display, egl_config, egl_pbuffer_attribs);
                egl_surface == EGL_NO_SURFACE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreatePbufferSurface() failed");
                return;
            }
        } else {
            if (EGLint egl_num_configs{}; eglChooseConfig(egl_display,
                                                          (m_opengl_mode == mode::opengl_es) ? egl_attribs_es.data() : egl_attribs.data(), &egl_config, 1,
                                                          &egl_num_configs) != EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglChooseConfig() failed");
                return;
            }
        }

        if (!create_context(nullptr, nullptr)) {
            return;
        }

        if (m_opengl_mode == mode::opengl_es && eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglMakeCurrent() failed");
            return;
        }
    }

    bool gl_context_egl::init_surface() {
        prepare_render_window();

        if (!render_window) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "surface is nullptr");
            return false;
        }

        destroy_surface();
        create_surface();

        if (eglSurfaceAttrib(egl_display, egl_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglSurfaceAttrib() failed");
            return false;
        }

        if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglMakeCurrent() failed");
            return false;
        }

        return true;
    }

    void gl_context_egl::update_surface(void *new_surface) {
        render_window = new_surface;

        clear_current();
        destroy_surface();
        create_surface();
        make_current();
    }

    bool gl_context_egl::make_current() {
        if (!egl_surface) {
            if (!init_surface()) {
                // Bind with no surface
                return eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
            } else {
                return true;
            }
        }
        return eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
    }

    bool gl_context_egl::clear_current() {
        return eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    void gl_context_egl::swap_buffers() {
        if (render_window) {
            eglSwapBuffers(egl_display, egl_surface);
        }
    }

    void gl_context_egl::set_swap_interval(const std::int32_t interval) {
        if (render_window) {
            eglSwapInterval(egl_display, interval);
        }
    }

    gl_context_egl::gl_context_egl(const window_system_info& wsi, bool stereo, bool core, bool is_gles)
        : egl_surface(nullptr)
        , egl_context(nullptr)
        , egl_display(nullptr)
        , render_window(nullptr)
        , wsi(wsi) {
        m_opengl_mode = is_gles ? mode::opengl_es : mode::opengl;
        init_gl();
    }

    gl_context_egl::~gl_context_egl() {
        destroy_surface();

        if (egl_context) {
            if (eglGetCurrentContext() == egl_context) {
                clear_current();
            }

            if (!eglDestroyContext(egl_display, egl_context)) {
                LOG_ERROR(DRIVER_GRAPHICS, "Failed to destroy GLES context!");
            }
        }
    }

    void gl_context_egl::create_surface() {
        if (!render_window) {
            return;
        }

        if (egl_surface = eglCreateWindowSurface(egl_display, egl_config, reinterpret_cast<EGLNativeWindowType>(render_window), 0);
            egl_surface == EGL_NO_SURFACE) {
            LOG_ERROR(DRIVER_GRAPHICS, "Create window surface failed with code: {}", eglGetError());
            return;
        }

        EGLint surface_width = 1;
        EGLint surface_height = 1;

        if (!eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &surface_width) ||
            !eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &surface_height))  {
            LOG_ERROR(DRIVER_GRAPHICS, "Failed to retrieve surface width or height.");
            return;
        }

        m_backbuffer_width = static_cast<int>(surface_width);
        m_backbuffer_height = static_cast<int>(surface_height);
    }

    void gl_context_egl::destroy_surface() {
        if (!egl_surface) {
            return;
        }
        if (eglGetCurrentSurface(EGL_DRAW) == egl_surface) {
            eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        if (!eglDestroySurface(egl_display, egl_surface)) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglDestroySurface() failed");
        }
        egl_surface = EGL_NO_SURFACE;
    }

    bool gl_context_egl::init_gl_for_shared(gl_context_egl *parent) {
        egl_display = parent->egl_display;
        egl_config = parent->egl_config;

        m_opengl_mode = parent->m_opengl_mode;

        if (eglBindAPI((m_opengl_mode == mode::opengl_es) ? EGL_OPENGL_ES_API : EGL_OPENGL_API) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "Can't bind target graphics API!");
            return false;
        }

        if (!create_context(parent->egl_context, &parent->context_version)) {
            return false;
        }

        return true;
    }

    std::unique_ptr<gl_context> gl_context_egl::create_shared_context() {
        std::unique_ptr<gl_context_egl> shared_context = std::make_unique<gl_context_egl>();
        if (!shared_context->init_gl_for_shared(this)) {
            return nullptr;
        }

        return shared_context;
    }

    void gl_context_egl::update(const std::uint32_t new_width, const std::uint32_t new_height) {
        m_backbuffer_width = static_cast<int>(new_width);
        m_backbuffer_height = static_cast<int>(new_height);
    }
}