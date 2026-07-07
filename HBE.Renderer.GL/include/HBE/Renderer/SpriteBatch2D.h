// HBE.Renderer.GL/include/HBE/Renderer/SpriteBatch2D.h
#pragma once

#include "HBE/Renderer/RenderPass.h"

#include <vector>
#include <cstdint>

namespace HBE::Renderer {

    class Material;
    class Mesh;
    class GLShader;
    class Texture2D;
    struct RenderItem;

    class SpriteBatch2D {
    public:
        SpriteBatch2D() = default;
        ~SpriteBatch2D();

        SpriteBatch2D(const SpriteBatch2D&) = delete;
        SpriteBatch2D& operator=(const SpriteBatch2D&) = delete;

        void setQuadMesh(const Mesh* quadMesh) { m_quadMesh = quadMesh; }

        void begin();
        void submit(const RenderItem& item); // pass is read from item.pass
        void flush(const float* viewProj);

        int drawCalls()      const { return m_drawCalls; }
        int quadCount()      const { return m_quadsSubmitted; }
        int stateChanges()   const { return m_stateChanges; } // NEW

    private:
        struct Quad {
            std::uint64_t sortKey = 0;
            std::uint32_t order = 0;
            const Material* material = nullptr;
            // 6 verts * (x, y, z, u, v, r, g, b, a) = 54 floats
            float v[6 * 9];
        };

        static std::uint64_t buildSortKey(const RenderItem& item, const Material& mat);
        static bool          quadLess(const Quad& a, const Quad& b);

        void initGL();
        void destroyGL();

        void emitQuadVertices(const RenderItem& item, float out54[54]) const;

        bool applyStateDiff(const Material* mat, const float* viewProj);

        void resetStateCache();

        const Mesh* m_quadMesh = nullptr;

        bool         m_glInited = false;
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;

        std::vector<Quad>  m_quads;
        std::vector<float> m_vertexStaging;

        int m_maxQuadsPerFlush = 5000;

        const GLShader* m_lastShader = nullptr;
        const Texture2D* m_lastTexture = nullptr;
        const Material* m_lastMaterial = nullptr;
        BlendMode        m_lastBlend = BlendMode::Invalid;

        std::uint32_t m_orderCounter = 0;
        int           m_drawCalls = 0;
        int           m_quadsSubmitted = 0;
        int           m_stateChanges = 0;
    };
}