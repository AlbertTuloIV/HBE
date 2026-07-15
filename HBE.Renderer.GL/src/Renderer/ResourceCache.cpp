#include "HBE/Renderer/ResourceCache.h"

#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Core/Log.h"

namespace HBE::Renderer {

	using HBE::Core::LogError;
	using HBE::Core::LogInfo;

	GLShader* ResourceCache::getOrCreateShader(const std::string& name, const char* vertexSrc, const char* fragmentSrc) {
		auto it = m_shaders.find(name);
		if (it != m_shaders.end()) {
			return it->second.get();
		}

		auto shader = std::make_unique<GLShader>();
		if (!shader->createFromSource(vertexSrc, fragmentSrc)) {
			LogError("ResourceCache: failed to create shader '" + name + "'");
			return nullptr;
		}

		GLShader* raw = shader.get();
		m_shaders.emplace(name, std::move(shader));
		return raw;
	}

	GLShader* ResourceCache::getOrCreateShaderFromFiles(const std::string& name,
		const std::string& vertexPath,
		const std::string& fragmentPath)
	{
		auto it = m_shaders.find(name);
		if (it != m_shaders.end()) {
			// Track paths anyway
			m_shaderFiles[name] = { vertexPath, fragmentPath };
			return it->second.get();
		}

		auto shader = std::make_unique<GLShader>();
		if (!shader->createFromFiles(vertexPath, fragmentPath)) {
			LogError("ResourceCache: failed to create shader '" + name + "' from files.");
			return nullptr;
		}

		GLShader* raw = shader.get();
		m_shaders.emplace(name, std::move(shader));
		m_shaderFiles[name] = { vertexPath, fragmentPath };
		return raw;
	}

	bool ResourceCache::reloadShader(const std::string& name) {
		auto it = m_shaders.find(name);
		if (it == m_shaders.end()) return false;

		GLShader* sh = it->second.get();

		// Prefer the shader's own stored paths.
		if (sh->reloadFromFiles()) return true;

		// Fallback to tracked paths (in case shader was created from source)
		auto pit = m_shaderFiles.find(name);
		if (pit == m_shaderFiles.end()) return false;

		return sh->createFromFiles(pit->second.first, pit->second.second);
	}

	Texture2D* ResourceCache::getOrCreateCheckerTexture(const std::string& name, int width, int height) {
		auto it = m_textures.find(name);
		if (it != m_textures.end()) {
			return it->second.get();
		}

		auto tex = std::make_unique<Texture2D>();
		if (!tex->createChecker(width, height)) {
			LogError("ResourceCache: failed to create checker texture '" + name + "'");
			return nullptr;
		}

		Texture2D* raw = tex.get();
		m_textures.emplace(name, std::move(tex));
		return raw;
	}

	Mesh* ResourceCache::getOrCreateMeshPosColor(const std::string& name,
		const std::vector<float>& vertices,
		int vertexCount)
	{
		auto it = m_meshes.find(name);
		if (it != m_meshes.end()) {
			return it->second.get();
		}

		auto mesh = std::make_unique<Mesh>();
		if (!mesh->create(vertices, vertexCount)) {
			LogError("ResourceCache: failed to create pos+color mesh '" + name + "'");
			return nullptr;
		}

		Mesh* raw = mesh.get();
		m_meshes.emplace(name, std::move(mesh));
		return raw;
	}

	Mesh* ResourceCache::getOrCreateMeshPosUV(const std::string& name, const std::vector<float>& vertices, int vertexCount) {
		auto it = m_meshes.find(name);
		if (it != m_meshes.end()) {
			return it->second.get();
		}

		auto mesh = std::make_unique<Mesh>();
		if (!mesh->createPostUV(vertices, vertexCount)) {
			LogError("ResourceCache: failed to create pos+UV mesh '" + name + "'");
			return nullptr;
		}

		Mesh* raw = mesh.get();
		m_meshes.emplace(name, std::move(mesh));
		return raw;
	}

	GLShader* ResourceCache::getShader(const std::string& name) const {
		auto it = m_shaders.find(name);
		if (it == m_shaders.end()) return nullptr;
		return it->second.get();
	}

	Texture2D* ResourceCache::getTexture(const std::string& name) const {
		auto it = m_textures.find(name);
		if (it == m_textures.end()) return nullptr;
		return it->second.get();
	}

	Texture2D* ResourceCache::getOrCreateTextureFromFile(const std::string& name, const std::string& path)
	{
		auto it = m_textures.find(name);
		if (it != m_textures.end()) {
			m_textureFiles[name] = path;
			return it->second.get();
		}

		auto tex = std::make_unique<Texture2D>();
		if (!tex->loadFromFile(path)) {
			LogError("ResourceCache: failed to load texture '" + name + "' from '" + path + "'");
			return nullptr;
		}

		Texture2D* raw = tex.get();
		m_textures.emplace(name, std::move(tex));
		m_textureFiles[name] = path;
		return raw;
	}

	bool ResourceCache::reloadTexture(const std::string& name) {
		auto it = m_textures.find(name);
		if (it == m_textures.end()) return false;

		auto pit = m_textureFiles.find(name);
		if (pit == m_textureFiles.end()) return false;

		Texture2D* tex = it->second.get();
		const std::string& path = pit->second;

		const bool ok = tex->loadFromFile(path);
		if (ok) LogInfo("Texture hot reloaded: " + name + " <- " + path);
		else    LogError("Texture reload failed: " + name + " <- " + path);
		return ok;
	}

	Texture2D* ResourceCache::getOrCreateTextureFromRGBA(
		const std::string& name,
		int width,
		int height,
		const unsigned char* rgbaPixels)
	{
		auto it = m_textures.find(name);
		if (it != m_textures.end()) {
			return it->second.get();
		}

		auto tex = std::make_unique<Texture2D>();
		if (!tex->createFromRGBA(width, height, rgbaPixels)) {
			LogError("ResourceCache: failed to create RGBA texture '" + name + "'");
			return nullptr;
		}

		Texture2D* raw = tex.get();
		m_textures.emplace(name, std::move(tex));
		return raw;
	}

	Texture2D* ResourceCache::placeholderTexture() {
		if (Texture2D* t = getTexture("__hbe_missing__")) return t;
		if (Texture2D* t = getOrCreateCheckerTexture("__hbe_missing__", 16, 16)) return t;
		// Absolute last resort: 1x1 magenta.
		const unsigned char magenta[4] = { 255, 0, 255, 255 };
		return getOrCreateTextureFromRGBA("__hbe_missing__", 1, 1, magenta);
	}

	Mesh* ResourceCache::getMesh(const std::string& name) const {
		auto it = m_meshes.find(name);
		if (it == m_meshes.end()) return nullptr;
		return it->second.get();
	}

} // namespace HBE::Renderer