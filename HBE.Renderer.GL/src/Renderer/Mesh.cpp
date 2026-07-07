#include "HBE/Core/Log.h"

#include "HBE/Renderer/Mesh.h"
#include <glad/glad.h>
#include <string>

namespace HBE::Renderer {

    Mesh::~Mesh() {
        destroy();
    }

    void Mesh::destroy() {
        if (m_vbo) {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        if (m_vao) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
    }

    bool Mesh::create(const std::vector<float>& vertices, int vertexCount) {
        // Validate: expects position(3) + color(3) = 6 floats per vertex
        constexpr int STRIDE = 6;
        const size_t expectedSize = static_cast<size_t>(vertexCount) * STRIDE;
        if (vertices.size() < expectedSize) {
            HBE::Core::LogError("Mesh::create: vertex buffer too small. Expected " + std::to_string(expectedSize) + " floats, got " + std::to_string(vertices.size()));
            return false;
        }
        if (vertexCount <= 0) {
            HBE::Core::LogError("Mesh::create: vertexCount must be positive");
            return false;
        }

        destroy();

        m_vertexCount = vertexCount;

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(float),
            vertices.data(),
            GL_STATIC_DRAW);

        // position (location = 0)
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            6 * sizeof(float),
            (void*)0
        );
        glEnableVertexAttribArray(0);

        // color (location = 1)
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE,
            6 * sizeof(float),
            (void*)(3 * sizeof(float))
        );
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    bool Mesh::createPostUV(const std::vector<float>& vertices, int vertexCount) {
        destroy();

        m_vertexCount = vertexCount;

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        // position (location = 0) -> 3 floats
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            5 * sizeof(float),
            (void*)0
        );
        glEnableVertexAttribArray(0);

        // UV (location = 1) -> 2 floats
        glVertexAttribPointer(
            1, 2, GL_FLOAT, GL_FALSE,
            5 * sizeof(float),
            (void*)(3 * sizeof(float))
        );
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

} // namespace HBE::Renderer
