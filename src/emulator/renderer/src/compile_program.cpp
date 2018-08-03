#include "functions.h"

#include "profile.h"

#include <renderer/types.h>

#include <gxm/types.h>
#include <util/log.h>

#include <vector>

namespace renderer {
    static SharedGLObject compile_glsl(GLenum type, const std::string &source) {
        R_PROFILE(__func__);

        const SharedGLObject shader = std::make_shared<GLObject>();
        if (!shader->init(glCreateShader(type), glDeleteShader)) {
            return SharedGLObject();
        }

        const GLchar *source_glchar = static_cast<const GLchar *>(source.c_str());
        const GLint length = static_cast<GLint>(source.length());
        glShaderSource(shader->get(), 1, &source_glchar, &length);

        glCompileShader(shader->get());

        GLint log_length = 0;
        glGetShaderiv(shader->get(), GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 0) {
            std::vector<GLchar> log;
            log.resize(log_length);
            glGetShaderInfoLog(shader->get(), log_length, nullptr, log.data());

            LOG_ERROR("{}", log.data());
        }

        GLboolean is_compiled = GL_FALSE;
        glGetShaderiv(shader->get(), GL_COMPILE_STATUS, &is_compiled);
        assert(is_compiled != GL_FALSE);
        if (!is_compiled) {
            return SharedGLObject();
        }

        return shader;
    }

    static void bind_attribute_locations(GLuint gl_program, const VertexProgram &program) {
        R_PROFILE(__func__);

        for (const AttributeLocations::value_type &binding : program.attribute_locations) {
            glBindAttribLocation(gl_program, binding.first / sizeof(uint32_t), binding.second.c_str());
        }
    }

    SharedGLObject compile_program(ProgramCache &cache, const GxmContextState &state, const MemState &mem) {
        R_PROFILE(__func__);

        assert(state.fragment_program);
        assert(state.vertex_program);

        const FragmentProgram &fragment_program = *state.fragment_program.get(mem)->renderer.get();
        const VertexProgram &vertex_program = *state.vertex_program.get(mem)->renderer.get();
        const ProgramGLSLs glsls(fragment_program.glsl, vertex_program.glsl);
        const ProgramCache::const_iterator cached = cache.find(glsls);
        if (cached != cache.end()) {
            return cached->second;
        }

        const SharedGLObject fragment_shader = compile_glsl(GL_FRAGMENT_SHADER, fragment_program.glsl);
        if (!fragment_shader) {
            LOG_CRITICAL("Error in compiled fragment shader:\n{}", fragment_program.glsl);
            return SharedGLObject();
        }
        const SharedGLObject vertex_shader = compile_glsl(GL_VERTEX_SHADER, vertex_program.glsl);
        if (!vertex_shader) {
            LOG_CRITICAL("Error in compiled vertex shader:\n{}", vertex_program.glsl);
            return SharedGLObject();
        }

        const SharedGLObject program = std::make_shared<GLObject>();
        if (!program->init(glCreateProgram(), &glDeleteProgram)) {
            return SharedGLObject();
        }

        glAttachShader(program->get(), fragment_shader->get());
        glAttachShader(program->get(), vertex_shader->get());

        bind_attribute_locations(program->get(), vertex_program);

        glLinkProgram(program->get());

        GLint log_length = 0;
        glGetProgramiv(program->get(), GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 0) {
            std::vector<GLchar> log;
            log.resize(log_length);
            glGetProgramInfoLog(program->get(), log_length, nullptr, log.data());

            LOG_ERROR("{}\n", log.data());
        }

        GLboolean is_linked = GL_FALSE;
        glGetProgramiv(program->get(), GL_LINK_STATUS, &is_linked);
        assert(is_linked != GL_FALSE);
        if (is_linked == GL_FALSE) {
            return SharedGLObject();
        }

        glDetachShader(program->get(), fragment_shader->get());
        glDetachShader(program->get(), vertex_shader->get());

        cache.emplace(glsls, program);

        return program;
    }
}
