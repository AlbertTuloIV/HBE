#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/Mesh.h"

namespace HBE::Renderer {

    class ResourceCache {
    public:
        ResourceCache() = default;
        ~ResourceCache() = default;

        ResourceCache(const ResourceCache&) = delete;
        ResourceCache& operator=(const ResourceCache&) = delete;

        // ----- Shaders -----
        GLShader* getOrCreateShader(const std::string& name,
            const char* vertexSrc,
            const char* fragmentSrc);

        // File-backed shader (hot reload friendly)
        GLShader* getOrCreateShaderFromFiles(const std::string& name,
            const std::string& vertexPath,
            const std::string& fragmentPath);

        bool reloadShader(const std::string& name);

        // ----- Textures -----
        Texture2D* getOrCreateCheckerTexture(const std::string& name,
            int width,
            int height);

        Texture2D* getOrCreateTextureFromFile(const std::string& name,
            const std::string& path);

        Texture2D* getOrCreateTextureFromRGBA(const std::string& name,
            int width,
            int height,
            const unsigned char* rgbaPixels);

        // Returns a shared 16x16 magenta/black checker used as the
        // "missing texture" placeholder. Never null (falls back to a
        // 1x1 magenta RGBA if even checker generation fails).
        Texture2D* placeholderTexture();

        bool reloadTexture(const std::string& name);

        // ----- Meshes -----
        Mesh* getOrCreateMeshPosColor(const std::string& name,
            const std::vector<float>& vertices,
            int vertexCount);

        Mesh* getOrCreateMeshPosUV(const std::string& name,
            const std::vector<float>& vertices,
            int vertexCount);

        // ----- Lookups -----
        GLShader* getShader(const std::string& name) const;
        Texture2D* getTexture(const std::string& name) const;
        Mesh* getMesh(const std::string& name) const;

        const std::unordered_map<std::string, std::string>& trackedTextureFiles() const { return m_textureFiles; }
        const std::unordered_map<std::string, std::pair<std::string, std::string>>& trackedShaderFiles() const { return m_shaderFiles; }

    private:
        std::unordered_map<std::string, std::unique_ptr<GLShader>>  m_shaders;
        std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_textures;
        std::unordered_map<std::string, std::unique_ptr<Mesh>>      m_meshes;

        // name -> path(s)
        std::unordered_map<std::string, std::string> m_textureFiles;
        std::unordered_map<std::string, std::pair<std::string, std::string>> m_shaderFiles;
    };

} // namespace HBE::Renderer