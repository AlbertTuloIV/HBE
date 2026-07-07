#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Texture2D.h"

#include <glad/glad.h>
#include <unordered_map>

namespace HBE::Renderer {

    namespace {
        template <typename T>
        std::uint16_t idFor(const T* ptr) {
            if (!ptr) return 0;
            static std::unordered_map<const void*, std::uint16_t> table;
            static std::uint16_t next = 1; // reserve 0 for null
            auto it = table.find(ptr);
            if (it != table.end()) return it->second;
            const std::uint16_t id = next++;
            table.emplace(ptr, id);
            return id;
        }
    }

    std::uint16_t Material::shaderId() const { return idFor<GLShader>(shader); }
    std::uint16_t Material::textureId() const { return idFor<Texture2D>(texture); }

    bool Material::uniformsEqual(const Material& other) const {
        if (useSDF != other.useSDF) return false;
        if (sdfSoftness != other.sdfSoftness) return false;
        return color.r == other.color.r
            && color.g == other.color.g
            && color.b == other.color.b
            && color.a == other.color.a;
    }

    void Material::apply(const float* mvp) const {
        if (!shader) return;

        // Bind shader and set MVP
        shader->use();
        shader->setMat4("uMVP", mvp);

        // Optional color uniform (if present)
        int colorLoc = shader->getUniformLocation("uColor");
        if (colorLoc >= 0) {
            glUniform4f(colorLoc, color.r, color.g, color.b, color.a);
        }

        // Optional SDF uniforms (if present)
        int isSdfLoc = shader->getUniformLocation("uIsSDF");
        if (isSdfLoc >= 0) {
            glUniform1i(isSdfLoc, useSDF ? 1 : 0);
        }

        int sdfSoftLoc = shader->getUniformLocation("uSDFSoftness");
        if (sdfSoftLoc >= 0) {
            glUniform1f(sdfSoftLoc, sdfSoftness);
        }

        // Texture + sampler (unit 0)
        if (texture) {
            glActiveTexture(GL_TEXTURE0);
            texture->bind();
            int texLoc = shader->getUniformLocation("uTex");
            if (texLoc >= 0) {
                glUniform1i(texLoc, 0);
            }
        }
    }

} // namespace HBE::Renderer
