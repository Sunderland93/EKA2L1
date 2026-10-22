#include <drivers/graphics/backend/ogl/common_ogl.h>

namespace eka2l1::drivers {
    ogl_es2_capabilities &ogl_es2_caps() {
        static ogl_es2_capabilities caps = {};
        return caps;
    }

    static GLuint es2_blit_program = 0;
    static GLuint es2_blit_vao = 0;
    static GLuint es2_blit_vbo = 0;

    static bool es2_blit_ensure_objects() {
        static bool attempted = false;
        static bool ready = false;

        if (attempted) {
            return ready;
        }

        attempted = true;

        static const char *vertex_source =
            "attribute vec2 a_pos;\n"
            "uniform vec4 u_src_rect;\n"
            "varying vec2 v_texcoord;\n"
            "void main() {\n"
            "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
            "    v_texcoord = u_src_rect.xy + ((a_pos * 0.5) + 0.5) * u_src_rect.zw;\n"
            "}\n";

        static const char *fragment_source =
            "precision mediump float;\n"
            "uniform sampler2D u_tex;\n"
            "varying vec2 v_texcoord;\n"
            "void main() {\n"
            "    gl_FragColor = texture2D(u_tex, v_texcoord);\n"
            "}\n";

        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &vertex_source, nullptr);
        glCompileShader(vertex_shader);

        GLint status = 0;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            glDeleteShader(vertex_shader);
            return false;
        }

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &fragment_source, nullptr);
        glCompileShader(fragment_shader);

        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            return false;
        }

        es2_blit_program = glCreateProgram();
        glAttachShader(es2_blit_program, vertex_shader);
        glAttachShader(es2_blit_program, fragment_shader);
        glBindAttribLocation(es2_blit_program, 0, "a_pos");
        glLinkProgram(es2_blit_program);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        glGetProgramiv(es2_blit_program, GL_LINK_STATUS, &status);
        if (!status) {
            glDeleteProgram(es2_blit_program);
            es2_blit_program = 0;
            return false;
        }

        static const GLfloat quad[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f
        };

        glGenVertexArrays(1, &es2_blit_vao);
        glBindVertexArray(es2_blit_vao);
        glGenBuffers(1, &es2_blit_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, es2_blit_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        ready = true;
        return ready;
    }

    bool ogl_es2_texture_blit(GLuint source_texture, const int texture_width, const int texture_height,
        const int source_x, const int source_y, const int source_w, const int source_h,
        const int dest_x, const int dest_y, const int dest_w, const int dest_h, const bool linear) {
        if (!source_texture || (texture_width <= 0) || (texture_height <= 0) || (source_w <= 0)
            || (source_h <= 0) || (dest_w <= 0) || (dest_h <= 0) || !ogl_es2_caps().has_vao) {
            return false;
        }

        if (!es2_blit_ensure_objects()) {
            return false;
        }

        {
            GLint current_program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);

            GLint prev_vao = 0;
            glGetIntegerv(EKA2L1_GL_VERTEX_ARRAY_BINDING_OES, &prev_vao);
            GLint prev_array_buffer = 0;
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_array_buffer);
            GLint prev_active_texture = 0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_texture);
            GLint prev_active_texture_binding = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_active_texture_binding);

            glActiveTexture(GL_TEXTURE0);
            GLint prev_texture = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);
            GLint prev_viewport[4] = { 0, 0, 0, 0 };
            glGetIntegerv(GL_VIEWPORT, prev_viewport);

            GLint prev_min_filter = GL_NEAREST;
            GLint prev_mag_filter = GL_NEAREST;
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &prev_min_filter);
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &prev_mag_filter);

            const GLboolean blend = glIsEnabled(GL_BLEND);
            const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
            const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
            const GLboolean cull = glIsEnabled(GL_CULL_FACE);
            const GLboolean stencil = glIsEnabled(GL_STENCIL_TEST);

            GLboolean depth_mask = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
            GLboolean color_mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
            glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);

            glDisable(GL_BLEND);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_STENCIL_TEST);
            glDepthMask(GL_FALSE);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            glUseProgram(es2_blit_program);
            glBindVertexArray(es2_blit_vao);
            glBindBuffer(GL_ARRAY_BUFFER, es2_blit_vbo);

            glBindTexture(GL_TEXTURE_2D, source_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);

            glUniform1i(glGetUniformLocation(es2_blit_program, "u_tex"), 0);
            glUniform4f(glGetUniformLocation(es2_blit_program, "u_src_rect"),
                static_cast<float>(source_x) / static_cast<float>(texture_width),
                static_cast<float>(source_y) / static_cast<float>(texture_height),
                static_cast<float>(source_w) / static_cast<float>(texture_width),
                static_cast<float>(source_h) / static_cast<float>(texture_height));

            glViewport(dest_x, dest_y, dest_w, dest_h);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, prev_min_filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, prev_mag_filter);

            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_texture));
            glActiveTexture(static_cast<GLenum>(prev_active_texture));
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_active_texture_binding));
            glBindVertexArray(static_cast<GLuint>(prev_vao));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(prev_array_buffer));
            glUseProgram(static_cast<GLuint>(current_program));

            glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

            if (blend) glEnable(GL_BLEND);
            if (depth_test) glEnable(GL_DEPTH_TEST);
            if (scissor) glEnable(GL_SCISSOR_TEST);
            if (cull) glEnable(GL_CULL_FACE);
            if (stencil) glEnable(GL_STENCIL_TEST);
            glDepthMask(depth_mask);
            glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
        }

        return true;
    }

    GLenum data_format_to_gl_enum(const data_format format) {
        switch (format) {
        case data_format::byte:
            return GL_UNSIGNED_BYTE;

        case data_format::sbyte:
            return GL_BYTE;

        case data_format::sword:
            return GL_SHORT;

        case data_format::word:
            return GL_UNSIGNED_SHORT;

        case data_format::uint:
            return GL_UNSIGNED_INT;

        case data_format::sint:
            return GL_INT;

        case data_format::sfloat:
            return GL_FLOAT;

        case data_format::fixed:
            return GL_FIXED;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }
    
    GLint texture_format_to_gl_enum(const texture_format format) {
        switch (format) {
        case texture_format::r:
            return GL_RED;

        case texture_format::r8:
            return GL_R8;

        case texture_format::rg:
            return GL_RG;

        case texture_format::rg8:
            return GL_RG8;

        case texture_format::rgb:
            return GL_RGB;

        case texture_format::bgr:
            return GL_BGR;

        case texture_format::bgra:
            return GL_BGRA;

        case texture_format::rgba:
            return GL_RGBA;

        case texture_format::rgba4:
            return GL_RGBA4;

        case texture_format::rgb565:
            return GL_RGB565;

        case texture_format::rgb5_a1:
            return GL_RGB5_A1;

        case texture_format::depth24_stencil8:
            return GL_DEPTH24_STENCIL8;

        case texture_format::depth_stencil:
            return GL_DEPTH_STENCIL;

        case texture_format::depth16:
            return GL_DEPTH_COMPONENT16;

        case texture_format::stencil8:
            return GL_STENCIL_INDEX8;

        case texture_format::etc2_rgb8:
            return GL_COMPRESSED_RGB8_ETC2;
        
        case texture_format::pvrtc_4bppv1_rgba:
            return GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;

        case texture_format::pvrtc_2bppv1_rgba:
            return GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;

        case texture_format::pvrtc_4bppv1_rgb:
            return GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;

        case texture_format::pvrtc_2bppv1_rgb:
            return GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;

        default:
            break;
        }

        return 0;
    }

    GLint texture_data_type_to_gl_enum(const texture_data_type data_type) {
        switch (data_type) {
        case texture_data_type::ubyte:
            return GL_UNSIGNED_BYTE;

        case texture_data_type::ushort:
            return GL_UNSIGNED_SHORT;

        case texture_data_type::uint_24_8:
            return GL_UNSIGNED_INT_24_8;

        case texture_data_type::ushort_4_4_4_4:
            return GL_UNSIGNED_SHORT_4_4_4_4;

        case texture_data_type::ushort_5_6_5:
            return GL_UNSIGNED_SHORT_5_6_5;

        case texture_data_type::ushort_5_5_5_1:
            return GL_UNSIGNED_SHORT_5_5_5_1;

        default:
            break;
        }

        return 0;
    }

    GLint to_filter_option(const filter_option op) {
        switch (op) {
        case filter_option::linear:
            return GL_LINEAR;

        case filter_option::nearest:
            return GL_NEAREST;

        case filter_option::linear_mipmap_linear:
            return GL_LINEAR_MIPMAP_LINEAR;

        case filter_option::linear_mipmap_nearest:
            return GL_LINEAR_MIPMAP_NEAREST;

        case filter_option::nearest_mipmap_linear:
            return GL_NEAREST_MIPMAP_LINEAR;

        case filter_option::nearest_mipmap_nearest:
            return GL_NEAREST_MIPMAP_NEAREST;

        default:
            break;
        }

        return 0;
    }
    
    GLenum to_tex_parameter_enum(const addressing_direction dir) {
        switch (dir) {
        case addressing_direction::s:
            return GL_TEXTURE_WRAP_S;

        case addressing_direction::t:
            return GL_TEXTURE_WRAP_T;

        case addressing_direction::r:
            return GL_TEXTURE_WRAP_R;

        default:
            break;
        }

        return 0;
    }

    GLint to_tex_wrapping_enum(const addressing_option opt) {
        switch (opt) {
        case addressing_option::clamp_to_edge:
            return GL_CLAMP_TO_EDGE;

        case addressing_option::repeat:
            return GL_REPEAT;

        default:
            break;
        }

        return 0;
    }
}