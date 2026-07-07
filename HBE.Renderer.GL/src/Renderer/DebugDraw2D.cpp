#include "HBE/Renderer/DebugDraw2D.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/GLShader.h"
#include "HBE/Core/Log.h"

#include <glad/glad.h>

namespace HBE::Renderer {

    using HBE::Core::LogError;

    bool DebugDraw2D::initialize(ResourceCache& cache, Mesh* quadMesh) {
        if (!quadMesh) {
            LogError("DebugDraw2D::initialize: quadMesh is null");
            return false;
        }

        m_quadMesh = quadMesh;

        // A tiny solid-color shader (no texture)
        const char* vs = R"(#version 330 core
            layout(location = 0) in vec3 aPos;
            uniform mat4 uMVP;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
            }
        )";

        const char* fs = R"(#version 330 core
            out vec4 FragColor;
            uniform vec4 uColor;
            void main() {
                FragColor = uColor;
            }
        )";

        m_shader = cache.getOrCreateShader("debug_solid_color", vs, fs);
        if (!m_shader) {
            LogError("DebugDraw2D::initialize: failed to create debug shader");
            return false;
        }

        m_mat.shader = m_shader;
        m_mat.texture = nullptr; // no texture needed

        return true;
    }

    void DebugDraw2D::rect(Renderer2D& r2d,
        float cx, float cy,
        float w, float h,
        float r, float g, float b, float a,
        bool filled)
    {
        if (!m_quadMesh || !m_shader) return;

        RenderItem item{};
        item.mesh = m_quadMesh;
        item.material = &m_mat;

        item.layer = 9000; // always above world sprites

        item.transform.posX = cx;
        item.transform.posY = cy;
        item.transform.scaleX = w;
        item.transform.scaleY = h;
        item.transform.rotation = 0.0f;

        m_mat.color = HBE::Renderer::Color4{ r, g, b, a };

        // Save current state (so we don�t break the rest of the engine)
        GLint prevMode[2];
        glGetIntegerv(GL_POLYGON_MODE, prevMode);

        glPolygonMode(GL_FRONT_AND_BACK, filled ? GL_FILL : GL_LINE);
        r2d.drawDirect(item);   // immediate draw — each rect gets its own color

        // Restore previous state
        glPolygonMode(GL_FRONT_AND_BACK, prevMode[0]);
    }



} // namespace HBE::Renderer
