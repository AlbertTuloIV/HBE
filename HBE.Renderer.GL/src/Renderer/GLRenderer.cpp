#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/PostProcessStack.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"
#include "HBE/Platform/SDLPlatform.h"

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <cmath>

namespace HBE::Renderer {

    using HBE::Core::LogError;
    using HBE::Core::LogInfo;

    // Build a 2D transform matrix (T * R * S) in column-major order
    void GLRenderer::buildTransformMatrix(const Transform2D& t, float out[16]) {
        float c = std::cos(t.rotation);
        float s = std::sin(t.rotation);
        float sx = t.scaleX;
        float sy = t.scaleY;

        out[0] = c * sx;  out[4] = -s * sy; out[8] = 0.0f; out[12] = t.posX;
        out[1] = s * sx;  out[5] = c * sy; out[9] = 0.0f; out[13] = t.posY;
        out[2] = 0.0f;    out[6] = 0.0f;    out[10] = 1.0f; out[14] = 0.0f;
        out[3] = 0.0f;    out[7] = 0.0f;    out[11] = 0.0f; out[15] = 1.0f;
    }

    bool GLRenderer::initialize(HBE::Platform::SDLPlatform& platform) {
        SDL_GLContext ctx = platform.getGLContext();
        SDL_Window* win = platform.getWindow();

        if (!ctx || !win) {
            LogError("GLRenderer::initialize: missing window or GL context");
            return false;
        }

        // Ensure this context is current
        if (SDL_GL_GetCurrentContext() != ctx) {
            if (!SDL_GL_MakeCurrent(win, ctx)) {
                LogError("SDL_GL_MakeCurrent in GLRenderer failed.");
                return false;
            }
        }

        // Load OpenGL functions via GLAD
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            LogError("gladLoadGLLoader failed: could not load OpenGL function.");
            return false;
        }

        int major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);

        LogInfo("OpenGL initialized via GLAD. Version " + std::to_string(major) + "." + std::to_string(minor));


        // --- NEW: enable alpha blending so transparent pixels stay transparent ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // NEW: use the actual window size
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(win, &w, &h);
        if (w <= 0 || h <= 0) {
            w = 1280;
            h = 720;
        }
        resizeViewport(w, h);

        glDisable(GL_DEPTH_TEST);

        m_initialized = true;
        return true;
    }

    void GLRenderer::beginFrame() {
        if (!m_initialized) return;

        glClearColor(
            m_clearColor[0],
            m_clearColor[1],
            m_clearColor[2],
            m_clearColor[3]
        );
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void GLRenderer::endFrame(HBE::Platform::SDLPlatform& platform) {
        if (!m_initialized) return;

        if (m_postProcess && m_postProcess->isInitialized()) {
            m_postProcess->present(m_vpX, m_vpY, m_vpW, m_vpH);
        }

        platform.swapBuffers();
    }

    void GLRenderer::setClearColor(float r, float g, float b, float a) {
        m_clearColor = { r, g, b, a };
    }

    void GLRenderer::setCamera(const Camera2D& cam) {
        // Just store a non-owning pointer; sandbox keeps ownership
        m_camera = const_cast<Camera2D*>(&cam);
    }

    void GLRenderer::buildViewMatrix(float out[16]) const {
        float cx = 0.0f;
        float cy = 0.0f;

        if (m_camera) {
            cx = m_camera->x;
            cy = m_camera->y;
        }

        out[0] = 1.0f; out[4] = 0.0f; out[8] = 0.0f; out[12] = -cx;
        out[1] = 0.0f; out[5] = 1.0f; out[9] = 0.0f; out[13] = -cy;
        out[2] = 0.0f; out[6] = 0.0f; out[10] = 1.0f; out[14] = 0.0f;
        out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
    }

    void GLRenderer::buildOrthoProjection(float out[16]) const {
        float w = 2.0f;
        float h = 2.0f;
        float zoom = 1.0f;

        if (m_camera) {
            w = m_camera->viewportWidth;
            h = m_camera->viewportHeight;
            zoom = m_camera->zoom;
        }

        float halfW = 0.5f * w / zoom;
        float halfH = 0.5f * h / zoom;

        float left = -halfW;
        float right = halfW;
        float bottom = -halfH;
        float top = halfH;
        float zNear = -1.0f;
        float zFar = 1.0f;

        const float rl = (right - left);
        const float tb = (top - bottom);
        const float fn = (zFar - zNear);

        out[0] = 2.0f / rl; out[1] = 0.0f;      out[2] = 0.0f;       out[3] = 0.0f;
        out[4] = 0.0f;      out[5] = 2.0f / tb; out[6] = 0.0f;       out[7] = 0.0f;
        out[8] = 0.0f;      out[9] = 0.0f;      out[10] = -2.0f / fn; out[11] = 0.0f;
        out[12] = -(right + left) / rl;
        out[13] = -(top + bottom) / tb;
        out[14] = -(zFar + zNear) / fn;
        out[15] = 1.0f;
    }

    void GLRenderer::multiplyMat4(const float* a, const float* b, float* out) const {
        // column-major: out = a * b
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += a[k * 4 + row] * b[col * 4 + k];
                }
                out[col * 4 + row] = sum;
            }
        }
    }

    void GLRenderer::resizeViewport(int width, int height) {
        if (!m_initialized) return;
        if (width <= 0 || height <= 0) return;

        glViewport(0, 0, width, height);
    }

    void GLRenderer::draw(const RenderItem& item) {
        if (!m_initialized || !item.mesh || !item.material)
            return;

        float model[16];
        float view[16];
        float proj[16];
        float vp[16];
        float mvp[16];

        buildTransformMatrix(item.transform, model);
        buildViewMatrix(view);
        buildOrthoProjection(proj);

        // vp = proj * view
        multiplyMat4(proj, view, vp);
        // mvp = vp * model
        multiplyMat4(vp, model, mvp);

        // Material applies shader, MVP, textures, etc.
        item.material->apply(mvp);

        // Optional UV rectangle (used by sprite sheets / quads later).
        if (item.material && item.material->shader) {
            int loc = item.material->shader->getUniformLocation("uUVRect");
            if (loc >= 0) {
                const float* r = item.uvRect;
                glUniform4f(loc, r[0], r[1], r[2], r[3]);
            }
        }

        glBindVertexArray(item.mesh->getVAO());
        glDrawArrays(GL_TRIANGLES, 0, item.mesh->getVertexCount());
        glBindVertexArray(0);
    }

    void GLRenderer::setViewportRect(int x, int y, int width, int height) {
        if (!m_initialized) return;
        if (width <= 0 || height <= 0) return;
        glViewport(x, y, width, height);
    }

    void GLRenderer::beginFrameFullWindow(int windowW, int windowH) {
        if (!m_initialized) return;
        if (windowW <= 0 || windowH <= 0) return;

        // Clear entire window to black for letterbox bars
        glViewport(0, 0, windowW, windowH);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void GLRenderer::beginFrameInViewport(int vpX, int vpY, int vpW, int vpH) {
        if (!m_initialized) return;
        if (vpW <= 0 || vpH <= 0) return;

        m_vpX = vpX; m_vpY = vpY; m_vpW = vpW; m_vpH = vpH;

        if (m_postProcess && m_postProcess->isInitialized()) {
            m_postProcess->bindSceneFBO();
            glViewport(0, 0, m_postProcess->sceneFBO().width(), m_postProcess->sceneFBO().height());
            glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else {
            glEnable(GL_SCISSOR_TEST);
            glViewport(vpX, vpY, vpW, vpH);
            glScissor(vpX, vpY, vpW, vpH);
            glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
        }
    }

    void GLRenderer::getViewProjection(float out16[16]) const {
        float view[16];
        float proj[16];
        buildViewMatrix(view);
        buildOrthoProjection(proj);

        // out = proj * view
        multiplyMat4(proj, view, out16);
    }


} // namespace HBE::Renderer
