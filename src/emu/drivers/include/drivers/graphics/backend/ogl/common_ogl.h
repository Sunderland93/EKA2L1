#pragma once

#include <drivers/graphics/common.h>
#include <common/platform.h>
#if EKA2L1_PLATFORM(IOS)
#include <drivers/graphics/backend/ogl/ios_gl_loader.h>
#else
#include <glad/glad.h>
#endif

namespace eka2l1::drivers {
    enum {
        GL_BACKEND_MAX_VBO_SLOTS = 10
    };

    // Core OpenGL ES 2.0 has no vertex array objects, instanced drawing,
    // framebuffer blit or base vertex draws. The N9's PowerVR SGX driver
    // exposes those through the OES/EXT variants, so the ES 2.0 backend
    // resolves them at runtime and records what is actually available.
    struct ogl_es2_capabilities {
        bool is_es2;
        bool has_vao;
        bool has_instancing;
        bool has_base_vertex;
        bool has_framebuffer_blit;
        bool has_etc1;
        bool has_draw_buffers;
    };

    ogl_es2_capabilities &ogl_es2_caps();

    // Constants that are not part of the GLES 2.0 core headers.
    enum {
        EKA2L1_GL_VERTEX_ARRAY_BINDING_OES = 0x85B5,
        EKA2L1_GL_ETC1_RGB8_OES = 0x8D64
    };

    /**
     * \brief Blit a 2D texture region into the currently bound framebuffer.
     *
     * GLES 2.0 has no glBlitFramebuffer, and GL_EXT_framebuffer_blit is not
     * guaranteed to be present. This draws a textured quad instead, which is
     * enough for the bitmap-resize and copy-to-texture paths. Viewport and
     * shader state are saved and restored.
     *
     * \returns True on success.
     */
    bool ogl_es2_texture_blit(GLuint source_texture, const int texture_width, const int texture_height,
        const int source_x, const int source_y, const int source_w, const int source_h,
        const int dest_x, const int dest_y, const int dest_w, const int dest_h, const bool linear);

    GLenum data_format_to_gl_enum(const data_format format);
    GLint texture_format_to_gl_enum(const texture_format format);
    GLint texture_data_type_to_gl_enum(const texture_data_type data_type);
    GLint to_filter_option(const filter_option op);
    GLenum to_tex_parameter_enum(const addressing_direction dir);
    GLint to_tex_wrapping_enum(const addressing_option opt);
}