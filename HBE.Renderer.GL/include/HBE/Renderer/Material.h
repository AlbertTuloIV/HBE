#pragma once

#include "HBE/Renderer/Color.h"
#include "HBE/Renderer/RenderPass.h"
#include <cstdint>

namespace HBE::Renderer {

    class GLShader;
    class Texture2D;

    // Holds "how to draw" info: shader, texture, tint color, etc.
    class Material {
    public:
        GLShader* shader = nullptr;
        Texture2D* texture = nullptr;

        // Basic tint color
        Color4 color{ 1.0f, 1.0f, 1.0f, 1.0f };

        // Optional: SDF font rendering controls.
        // For normal sprites/bitmaps, leave useSDF=false.
        bool useSDF = false;
        float sdfSoftness = 1.0f; // higher = softer edge; 1.0 is a good default

        // Which glBlendFunc to use when this material is bound
        BlendMode blend = BlendMode::Alpha;

        // Apply this material to the GPU, given an MVP matrix
        void apply(const float* mvp) const;

        // stable numeric IDSs handed out of first sight of a resource.
        // Used by SpriteBatch2D to build a 64-bit sort key without touching pointers.
        // Both return 0 when the resource pointer is null.
        std::uint16_t shaderId() const;
        std::uint16_t textureId() const;

        // Detect whether two materials would produce the same uniform state.
        // Shader/texture/blend differences are handled by the sort key itself.
        bool uniformsEqual(const Material& other) const;
    };

} // namespace HBE::Renderer
