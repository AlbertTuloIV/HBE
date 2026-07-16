#include "World/World.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/TileMapLoader.h"

#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>

using namespace HBE::Renderer;

namespace MegaX {

	bool World::load(Renderer2D& r2d,
		ResourceCache& resources,
		GLShader* spriteShader,
		Mesh* quadMesh,
		const std::string& logicalMapPath) {
		m_loaded = false;
		m_map = TileMap{};
		m_animatedTiles.clear();
		m_animClock = 0.0f;

		const std::string absPath = HBE::Core::AssetPaths::Resolve(logicalMapPath);

		std::string err;
		if (!TileMapLoader::loadFromJsonFile(absPath, m_map, &err)) {
			HBE::Core::LogError("World::load: failed to load '" + logicalMapPath + "': " + err);
			m_map = TileMap{};
			return false;
		}

		if (!m_renderer.build(r2d, resources, spriteShader, quadMesh, m_map)) {
			HBE::Core::LogError("World::load: TileMapRenderer::build failed for '" + logicalMapPath + "'.");
			return false;
		}

		m_loaded = true;
		HBE::Core::LogInfo("World loaded '" + logicalMapPath + "' ("
			+ std::to_string(m_map.tilesets.size()) + " tilesets, "
			+ std::to_string(m_map.layers.size()) + " layers).");
		return true;
	}

	void World::update(float dt) {
		m_animClock += dt;

	}

	void World::render(Renderer2D& r2d) {
		if (!m_loaded) return;
		m_renderer.draw(r2d, m_map);
		renderAnimatedTiles(r2d);
	}

    void World::renderAnimatedTiles(Renderer2D& r2d) {

	}

	float World::pixelWidth() const {
		if (!m_loaded || m_map.layers.empty()) return 0.0f;
		int maxW = 0;
		for (const auto& l : m_map.layers) maxW = std::max(maxW, l.w);
		return maxW * m_map.worldTileW();
	}

	float World::pixelHeight() const {
		if (!m_loaded || m_map.layers.empty()) return 0.0f;
		int maxH = 0;
		for (const auto& l : m_map.layers) maxH = std::max(maxH, l.h);
		return maxH * m_map.worldTileH();
	}
}