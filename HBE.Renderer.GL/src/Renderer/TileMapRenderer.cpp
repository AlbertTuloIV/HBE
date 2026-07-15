#include "HBE/Renderer/TileMapRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/RenderItem.h"

#include "HBE/Core/Log.h"

#include <cmath>

namespace HBE::Renderer {

    using HBE::Core::LogError;

    bool TileMapRenderer::build(Renderer2D&, ResourceCache& cache, GLShader* spriteShader, Mesh* quadMesh, const TileMap& map) {
        if (!spriteShader || !quadMesh) return false;

        m_quadMesh = quadMesh;
        m_tilesets.clear();
        m_tilesets.reserve(map.tilesets.size());

        for (auto& ts : map.tilesets) {
            TilesetDrawData d{};
            d.tileW = ts.tileW;
            d.tileH = ts.tileH;
            d.margin = ts.margin;
            d.spacing = ts.spacing;

            Texture2D* tex = cache.getOrCreateTextureFromFile("tileset_" + ts.name, ts.texturePath);
            if (!tex) {
                LogError("TileMapRenderer: failed to load tileset texture '" + ts.texturePath
                    + "' — using placeholder.");
                tex = cache.placeholderTexture();
                if (!tex) return false;
            }

            d.texW = tex->getWidth();
            d.texH = tex->getHeight();
            d.material.shader = spriteShader;
            d.material.texture = tex;

            m_tilesets.push_back(std::move(d));
        }
        return true;
    }

    void TileMapRenderer::computeTileUV(const TilesetDrawData& ts, int tileIndex, float outUV[4]) const {
        const int strideX = ts.tileW + ts.spacing;
        const int strideY = ts.tileH + ts.spacing;

        const int usableW = ts.texW - 2 * ts.margin;
        const int cols = (strideX > 0) ? (usableW / strideX) : 1;

        const int col = (cols > 0) ? (tileIndex % cols) : 0;
        const int row = (cols > 0) ? (tileIndex / cols) : 0;

        const int origX = ts.margin + col * strideX;
        const int origY = ts.margin + row * strideY;

        // stb is flipped vertically on load, so convert "top-left atlas coords" -> "bottom-left texture coords"
        const int loadedY = ts.texH - (origY + ts.tileH);

        // Half-texel inset to prevent sampling neighbors (atlas bleeding / seams)
        const float insetU = 0.5f / (float)ts.texW;
        const float insetV = 0.5f / (float)ts.texH;

        const float uMin = ((float)origX / (float)ts.texW) + insetU;
        const float vMin = ((float)loadedY / (float)ts.texH) + insetV;

        const float uMax = ((float)(origX + ts.tileW) / (float)ts.texW) - insetU;
        const float vMax = ((float)(loadedY + ts.tileH) / (float)ts.texH) - insetV;

        outUV[0] = uMin;
        outUV[1] = vMin;
        outUV[2] = (uMax - uMin); // scale
        outUV[3] = (vMax - vMin); // scale
    }

    void TileMapRenderer::draw(Renderer2D& r2d, const TileMap& map) {
        if (!m_quadMesh || m_tilesets.empty()) return;

        RenderItem item{};
        item.layer = 0;
        item.mesh = m_quadMesh;

        // Use integer pixel-aligned tile sizes to avoid subpixel seams.
        const float tw = std::round(map.worldTileW());
        const float th = std::round(map.worldTileH());
        if (tw <= 0.0f || th <= 0.0f) return;

        // camera culling
        int camMinX = 0, camMaxX = 0, camMinY = 0, camMaxY = 0;
        bool hasCam = false;

        if (const Camera2D* cam = r2d.activeCamera()) {
            const float halfW = 0.5f * cam->viewportWidth / std::max(cam->zoom, 0.0001f);
            const float halfH = 0.5f * cam->viewportHeight / std::max(cam->zoom, 0.0001f);

            // pad by 1 tile so edges don't pop
            const float left = cam->x - halfW - tw;
            const float right = cam->x + halfW + tw;
            const float bottom = cam->y - halfH - th;
            const float top = cam->y + halfH + th;

            camMinX = (int)std::floor(left / tw);
            camMaxX = (int)std::floor(right / tw);
            camMinY = (int)std::floor(bottom / th);
            camMaxY = (int)std::floor(top / th);

            hasCam = true;
        }

        for (const auto& layer : map.layers) {
            if (layer.tilesetIndex < 0 || layer.tilesetIndex >= (int)m_tilesets.size()) continue;

            auto& ts = m_tilesets[layer.tilesetIndex];
            item.material = &ts.material;

            item.transform.scaleX = tw;
            item.transform.scaleY = th;
            item.transform.rotation = 0.0f;

            // Compute culled tile bounds per-layer (clamped)
            int x0 = 0, x1 = layer.w - 1;
            int y0 = 0, y1 = layer.h - 1;

            if (hasCam) {
                x0 = std::max(0, camMinX);
                y0 = std::max(0, camMinY);
                x1 = std::min(layer.w - 1, camMaxX);
                y1 = std::min(layer.h - 1, camMaxY);
            }

            if (x0 > x1 || y0 > y1) continue;

            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    const int tileId = layer.at(x, y);
                    if (tileId == 0) continue;

                    // map tile ids are 1-based, atlas is 0-based
                    const int atlasIndex = tileId - 1;

                    computeTileUV(ts, atlasIndex, item.uvRect);

                    // Pixel-snap positions
                    float px = (float)x * tw + tw * 0.5f;
                    float py = (float)y * th + th * 0.5f;

                    px = std::round(px);
                    py = std::round(py);

                    item.transform.posX = px;
                    item.transform.posY = py;

                    r2d.draw(item);
                }
            }
        }
    }


} // namespace HBE::Renderer
