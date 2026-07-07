#pragma once

#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/RenderPass.h"
#include "HBE/Renderer/Color.h"

namespace HBE::Renderer {

    class Mesh;
    class Material;
    

    // One drawable thing in the scene
    struct RenderItem {
        Mesh* mesh = nullptr;
        Material* material = nullptr;
        Transform2D transform;

        // Draw order controls (used by SpriteBatch2D sorting).
        // lower layers draw first (behind). higher layers draw last (in front).
        int layer = 0;

        // optional secondary sort key within a lyaer (e.g. for top-down y-sorting).
        // higher values draw later when layer/material match.
        float sortKey = 0.0f;

        // which pass this item belongs to, defaults to world so existing code keeps working.
        RenderPass pass = RenderPass::World;

        // Per-item tint baked into the vertex color attribute. This lets many quads
        // with different colors share a single draw call (e.g. particles), because
        // color no longer forces a uniform change.
        Color4 tint{ 1.0f, 1.0f, 1.0f, 1.0f };

        // u0, v0, u1, v1  (normalized 0..1)
        float uvRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    };

} // namespace HBE::Renderer
