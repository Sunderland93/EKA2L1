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

#pragma once

#include <common/platform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

// EGL_OPENGL_ES3_BIT_KHR comes from EGL_KHR_create_context. Harmattan ships
// EGL 1.4 headers with eglext version 3, which predate the extension, and its
// implementation only ever offers ES 2 configurations. The token is spelled
// out here so the shared EGL backend keeps compiling; the device path asks for
// an ES 2 configuration instead.
#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

// Also from EGL_KHR_create_context / EGL 1.5. Only used on the desktop
// OpenGL path, which MeeGo never takes, but the shared code still has to
// compile on the device.
#ifndef EGL_CONTEXT_MAJOR_VERSION
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#endif
#ifndef EGL_CONTEXT_MINOR_VERSION
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#endif

// The PowerVR SGX in the N9 provides ES 2 configurations only.
#if EKA2L1_PLATFORM(MEEGO)
#define EKA2L1_EGL_RENDERABLE_TYPE EGL_OPENGL_ES2_BIT
#define EKA2L1_EGL_CONTEXT_CLIENT_VERSION 2
#else
#define EKA2L1_EGL_RENDERABLE_TYPE EGL_OPENGL_ES3_BIT_KHR
#define EKA2L1_EGL_CONTEXT_CLIENT_VERSION 3
#endif
