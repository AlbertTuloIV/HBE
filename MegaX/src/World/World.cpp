#include "World/World.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Texture2D.h"

#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

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
		m_spriteShader = spriteShader;
		m_quadMesh = quadMesh;

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

		loadAnimatedTiles(resources, absPath);

		m_loaded = true;
		size_t instanceCount = 0;
		for (const auto& a : m_animatedTiles) instanceCount += a.instances.size();

		HBE::Core::LogInfo("World loaded '" + logicalMapPath + "' ("
			+ std::to_string(m_map.tilesets.size()) + " tilesets, "
			+ std::to_string(m_map.layers.size()) + " layers, "
			+ std::to_string(m_animatedTiles.size()) + " animated tiles, "
			+ std::to_string(instanceCount) + " instances).");
		return true;
	}

	void World::loadAnimatedTiles(ResourceCache& resources, const std::string& mapAbsPath) {
		m_animatedTiles.clear();

		std::ifstream f(mapAbsPath);
		if (!f.is_open()) {
			HBE::Core::LogError("World: could not reopen map for animatedTiles: "
				+ mapAbsPath);
			return;
		}

		nlohmann::json j;
		try {
			f >> j;
		}
		catch (const std::exception& e) {
			HBE::Core::LogError(std::string("World: animatedTiles JSON parse error: ")
				+ e.what());
			return;
		}

		if (!j.contains("animatedTiles") || !j["animatedTiles"].is_array()) return;

		std::vector<AnimatedTile> defs;
		for (const auto& e : j["animatedTiles"]) {
			AnimatedTile a{};
			a.tilesetIndex = e.value("tileset", 0);
			a.tileId = e.value("tileId", 0);
			a.frameW = e.value("frameW", m_map.tileSizeW);
			a.frameCount = e.value("frames", 1);
			a.fps = e.value("fps", 8.0f);

			const std::string rel = e.value("sheet", std::string{});
			if (a.tileId <= 0 || a.frameCount < 0 || rel.empty()) {
				HBE::Core::LogError("World: skipping invalid animatedTiles entry.");
				continue;
			}

			a.sheetPath = HBE::Core::AssetPaths::ResolveRelativeTo(mapAbsPath, rel);

			const std::string cacheName = "animtile_ts" + std::to_string(a.tilesetIndex) + "_id" + std::to_string(a.tileId);

			a.texture = resources.getOrCreateTextureFromFile(cacheName, a.sheetPath);
			if (!a.texture) {
				HBE::Core::LogError("World: failed to load animated sheet' " + a.sheetPath + "'.");
				continue;
			}
			a.texW = a.texture->getWidth();
			a.texH = a.texture->getHeight();

			a.material.shader = m_spriteShader;
			a.material.texture = a.texture;

			for (const auto& layer : m_map.layers) {
				if (layer.tilesetIndex != a.tilesetIndex) continue;
				for (int y = 0; y < layer.h; ++y) {
					for (int x = 0; x < layer.w; ++x) {
						if (layer.at(x, y) == a.tileId) {
							a.instances.push_back(AnimatedTileInstance{ x, y });
						}
					}
				}			
			}
			if (a.instances.empty()) {
				HBE::Core::LogInfo("World: animated tile id "
					+ std::to_string(a.tileId) + " (tileset "
					+ std::to_string(a.tilesetIndex)
					+ ") is defined but not placed in any layer.");
			}
			defs.push_back(std::move(a));
		}
		m_animatedTiles = std::move(defs);
	}

	void World::update(float dt) {
		m_animClock += dt;

	}

	void World::render(Renderer2D& r2d) {
		if (!m_loaded) return;
		m_renderer.draw(r2d, m_map);
		renderAnimatedTiles(r2d);
	}

	void World::computeFrameUV(const AnimatedTile& a, int frame, float outUV[4]) const {
		const int cols = (a.frameW > 0) ? (a.texW / a.frameW) : 1;
		const int col = (cols > 0) ? (frame % cols) : 0;
		const int row = (cols > 0) ? (frame / cols) : 0;
		const int origX = col * a.frameW;
		const int origY = row * a.frameH;

		const int loadedY = a.texH - (origY + a.frameH);

		const float insetU = (a.texW > 0) ? 0.5f / (float)a.texW : 0.0f;
		const float insetV = (a.texH > 0) ? 0.5f / (float)a.texH : 0.0f;

		const float uMin = (float)origX / (float)a.texW + insetU;
		const float vMin = (float)loadedY / (float)a.texH + insetV;
		const float uMax = (float)(origX + a.frameW) / (float)a.texW - insetU;
		const float vMax = (float)(loadedY + a.frameH) / (float)a.texH - insetV;

		outUV[0] = uMin;
		outUV[1] = vMin;
		outUV[2] = uMax - uMin;
		outUV[3] = vMax - vMin;
	}

    void World::renderAnimatedTiles(Renderer2D& r2d) {
		if (m_animatedTiles.empty() || !m_quadMesh) return;

		const float tw = std::round(m_map.worldTileW());
		const float th = std::round(m_map.worldTileH());
		if (tw <= 0.0f || th <= 0.0f) return;

		RenderItem item{};
		item.mesh = m_quadMesh;
		item.pass = RenderPass::World;
		item.layer = 1;
		item.transform.scaleX = tw;
		item.transform.scaleY = th;

		for (auto& a : m_animatedTiles) {
			if (!a.texture || a.instances.empty() || a.frameCount <= 0) continue;

			const long long ticks = (long long)std::floor(m_animClock * a.fps);
			const int frame = (int)(((ticks % a.frameCount) + a.frameCount) % a.frameCount);

			computeFrameUV(a, frame, item.uvRect);
			item.material = &a.material;

			for (const auto& inst : a.instances) {
				item.transform.posX = std::round(inst.tileX * tw + tw * 0.5f);
				item.transform.posY = std::round(inst.tileY * th + th * 0.5f);
				r2d.draw(item);
			}
		}
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