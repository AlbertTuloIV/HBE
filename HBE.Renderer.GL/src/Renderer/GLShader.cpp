#include "HBE/Renderer/GLShader.h"
#include "HBE/Core/Log.h"

#include <SDL3/SDL.h>

#include <glad/glad.h>
#include <fstream>
#include <sstream>

namespace HBE::Renderer {

    using HBE::Core::LogError;
    using HBE::Core::LogInfo;

    GLShader::~GLShader() {
        destroy();
    }

    void GLShader::destroy() {
        if (m_program != 0) {
            if (SDL_GL_GetCurrentContext() != nullptr) {
                glDeleteProgram(m_program);
            }
            m_program = 0;
        }
    }

    bool GLShader::readAllText(const std::string& path, std::string& out) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::stringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    static bool compileShader(GLuint shader, const char* src, std::string& outLog) {
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            outLog.assign((size_t)std::max(0, length), '\0');
            if (length > 0) {
                glGetShaderInfoLog(shader, length, &length, outLog.data());
            }
            return false;
        }
        return true;
    }

    bool GLShader::buildProgramFromSource(const char* vs, const char* fs, unsigned int& outProgram) {
        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

        std::string vlog, flog;
        if (!compileShader(vert, vs, vlog)) {
            LogError("Shader compile error (vertex): " + vlog);
            glDeleteShader(vert);
            glDeleteShader(frag);
            return false;
        }
        if (!compileShader(frag, fs, flog)) {
            LogError("Shader compile error (fragment): " + flog);
            glDeleteShader(vert);
            glDeleteShader(frag);
            return false;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);

        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::string log((size_t)std::max(0, length), '\0');
            if (length > 0) {
                glGetProgramInfoLog(program, length, &length, log.data());
            }
            LogError("Program link error: " + log);
            glDeleteProgram(program);
            return false;
        }

        outProgram = program;
        return true;
    }

    bool GLShader::createFromSource(const char* vertexSrc, const char* fragmentSrc) {
        // Source shaders are not file-backed.
        m_vertexPath.clear();
        m_fragmentPath.clear();

        unsigned int program = 0;
        if (!buildProgramFromSource(vertexSrc, fragmentSrc, program)) {
            return false;
        }

        destroy();
        m_program = program;
        return true;
    }

    bool GLShader::createFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
        std::string vs, fs;
        if (!readAllText(vertexPath, vs)) {
            LogError("GLShader: could not read vertex shader file: " + vertexPath);
            return false;
        }
        if (!readAllText(fragmentPath, fs)) {
            LogError("GLShader: could not read fragment shader file: " + fragmentPath);
            return false;
        }

        unsigned int program = 0;
        if (!buildProgramFromSource(vs.c_str(), fs.c_str(), program)) {
            return false;
        }

        destroy();
        m_program = program;
        m_vertexPath = vertexPath;
        m_fragmentPath = fragmentPath;
        return true;
    }

    bool GLShader::reloadFromFiles() {
        if (m_vertexPath.empty() || m_fragmentPath.empty()) return false;

        std::string vs, fs;
        if (!readAllText(m_vertexPath, vs)) {
            LogError("GLShader: could not read vertex shader file: " + m_vertexPath);
            return false;
        }
        if (!readAllText(m_fragmentPath, fs)) {
            LogError("GLShader: could not read fragment shader file: " + m_fragmentPath);
            return false;
        }

        unsigned int newProgram = 0;
        if (!buildProgramFromSource(vs.c_str(), fs.c_str(), newProgram)) {
            // Keep old program
            return false;
        }

        // Swap programs (keep pointer stable)
        if (m_program != 0) {
            glDeleteProgram(m_program);
        }
        m_program = newProgram;

        LogInfo("GLShader hot reloaded: " + m_vertexPath + " + " + m_fragmentPath);
        return true;
    }

    int GLShader::getUniformLocation(const char* name) const {
        if (m_program == 0) return -1;
        return glGetUniformLocation(m_program, name);
    }

    bool GLShader::setMat4(const char* name, const float* value) const {
        if (m_program == 0) return false;
        int loc = glGetUniformLocation(m_program, name);
        if (loc < 0) {
            LogError(std::string("Uniform not found: ") + name);
            return false;
        }
        glUniformMatrix4fv(loc, 1, GL_FALSE, value); // column-major
        return true;
    }

    void GLShader::use() const {
        if (m_program != 0) {
            glUseProgram(m_program);
        }
    }

} // namespace HBE::Renderer