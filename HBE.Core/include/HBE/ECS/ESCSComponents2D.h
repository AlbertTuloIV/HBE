#pragma once

// Forward declare to avoid heavy includes and circular deps
namespace HBE::Renderer {
    class Mesh;
    class Material;
}

#include "HBE/Renderer/SpriteAnimationStateMachine.h"

namespace HBE::Renderer {

    // Sprite render component (RenderItem minus Transform)
    struct SpriteComponent2D {
        Mesh* mesh = nullptr;
        Material* material = nullptr;

        int layer = 0;
        float sortKey = 0.0f;

        // u0, v0, u1, v1
        float uvRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

        float sortOffsetY = 0.0f; // pixels/world units: negative moves pivot down (toward feet)

        bool visible = true;
    };


    // Animation component
    struct AnimationComponent2D {
        SpriteAnimationStateMachine sm;
    };

} // namespace HBE::Renderer