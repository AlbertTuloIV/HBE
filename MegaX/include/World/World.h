#pragma once

#include <string>
#include <vector>

#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileMapRenderer.h"

#include "World/AnimatedTile.h"

namespace HBE::Renderer {
	class Renderer2D;
	class ResourceCache;
	class GLShader;
	class Mesh;
}

namespace MegaX {

	class World {
	public:
		bool load(HBE::Renderer::Renderer2D& r2d,
			HBE::Renderer::ResourceCache& resources,
			HBE::Renderer::GLShader* spriteShader,
			HBE::Renderer::Mesh* quadMesh,
			const std::string& logicalMapPath);

		void update(float dt);

		void render(HBE::Renderer::Renderer2D& r2d);

		bool loaded() const { return m_loaded; }
		const HBE::Renderer::TileMap& map() const { return m_map; }

		float pixelWidth() const;
		float pixelHeight() const;

	private:
		void renderAnimatedTiles(HBE::Renderer::Renderer2D& r2d);

		HBE::Renderer::TileMap m_map{};
		HBE::Renderer::TileMapRenderer m_renderer{};
		bool m_loaded = false;

		std::vector<AnimatedTile> m_animatedTiles{};
		float m_animClock = 0.0f;
	};
}